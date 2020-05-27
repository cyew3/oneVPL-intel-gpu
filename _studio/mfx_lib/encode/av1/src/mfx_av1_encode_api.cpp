// Copyright (c) 2014-2020 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)
#include "assert.h"
#include "algorithm"

#include "vm_sys_info.h"

#include "mfx_av1_defaults.h"
#include "mfx_av1_encode_api.h"
#include "mfx_av1_encode.h"
#include "mfx_av1_core_iface_wrapper.h"
#include "mfx_av1_tables.h"

using namespace AV1Enc;
using namespace AV1Enc::MfxEnumShortAliases;

namespace {
    mfxStatus CheckExtBuffers(mfxExtBuffer **extParam, int32_t numExtParam)
    {
        if (extParam == NULL)
            return MFX_ERR_NONE;

        const ExtBuffers supported;
        const int32_t numSupported = sizeof(supported.extParamAll) / sizeof(supported.extParamAll[0]);
        int32_t found[numSupported] = {};

        for (int32_t i = 0; i < numExtParam; i++) {
            if (extParam[i] == NULL)
                return MFX_ERR_NULL_PTR;
            int32_t idx = 0;
            for (; idx < numSupported; idx++)
                if (supported.extParamAll[idx]->BufferId == extParam[i]->BufferId)
                    break;
            if (idx >= numSupported)
                return MFX_ERR_UNSUPPORTED;
            if (extParam[i]->BufferSz != supported.extParamAll[idx]->BufferSz)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (found[idx])
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            found[idx] = 1;
        }

        return MFX_ERR_NONE;
    }

    bool AllMandatedParamsPresent(const mfxVideoParam *par)
    {
        mfxExtHEVCRegion *region = GetExtBuffer(*par);
        if (region && region->RegionEncoding == MFX_HEVC_REGION_ENCODING_ON && region->RegionId > 0 && par->mfx.NumSlice == 0)
            return false;

        return par->IOPattern &&
            par->mfx.FrameInfo.Width && par->mfx.FrameInfo.Height &&
            par->mfx.FrameInfo.FourCC && par->mfx.FrameInfo.ChromaFormat &&
            par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD &&
            (par->mfx.RateControlMethod == CQP || par->mfx.TargetKbps);
    }

    template <class T> mfxExtBuffer MakeExtBufferHeader()
    {
        mfxExtBuffer header = {Type2Id<T>::id, sizeof(T)};
        return header;
    }

    template <class T> void InitExtBuffer0(T &buf)
    {
        buf.Header = MakeExtBufferHeader<T>();
        memset((uint8_t *)&buf + sizeof(mfxExtBuffer), 0, sizeof(T) - sizeof(mfxExtBuffer));
    }

    template <class T> void InitExtBuffer(T &buf)
    {
        InitExtBuffer0<T>(buf);
    }

    void InitExtBuffer(mfxExtHEVCRegion &buf)
    {
        InitExtBuffer0<mfxExtHEVCRegion>(buf);
        buf.RegionEncoding = MFX_HEVC_REGION_ENCODING_OFF;
    }

    bool MatchExtBuffers(const mfxVideoParam &par1, const mfxVideoParam &par2)
    {
        const mfxVideoParam *p1 = &par1, *p2 = &par2;
        for (uint32_t iter = 0; iter < 2; iter++, std::swap(p1, p2))
            if (p1->ExtParam)
                for (uint32_t i = 0; i < p1->NumExtParam; i++)
                    if (p1->ExtParam[i]->BufferId != MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION)
                        if (GetExtBufferById(p2->ExtParam, p2->NumExtParam, p1->ExtParam[i]->BufferId) == NULL)
                            return false;
        return true;
    }

    void CopySpsPps(mfxExtCodingOptionSPSPPS *dst, const mfxExtCodingOptionSPSPPS *src)
    {
        if (dst->SPSBuffer && src->SPSBuffer) {
            memmove(dst->SPSBuffer, src->SPSBuffer, src->SPSBufSize);
            dst->SPSBufSize = src->SPSBufSize;
        }
        if (dst->PPSBuffer && src->PPSBuffer) {
            memmove(dst->PPSBuffer, src->PPSBuffer, src->PPSBufSize);
            dst->PPSBufSize = src->PPSBufSize;
        }
        dst->SPSId = src->SPSId;
        dst->PPSId = src->PPSId;
    }

    void CopyVps(mfxExtCodingOptionVPS *dst, const mfxExtCodingOptionVPS *src)
    {
        if (dst->VPSBuffer && src->VPSBuffer) {
            memmove(dst->VPSBuffer, src->VPSBuffer, src->VPSBufSize);
            dst->VPSBufSize = src->VPSBufSize;
        }
        dst->VPSId = src->VPSId;
    }

    void CopyParam(mfxVideoParam &dst, const mfxVideoParam &src)
    {
        dst.AsyncDepth = src.AsyncDepth;
        dst.IOPattern = src.IOPattern;
        dst.Protected = src.Protected;
        dst.mfx = src.mfx;
        if (dst.ExtParam && src.ExtParam) {
            for (uint32_t i = 0; i < src.NumExtParam; i++) {
                if (mfxExtBuffer *d = GetExtBufferById(dst.ExtParam, dst.NumExtParam, src.ExtParam[i]->BufferId)) {
                    if (src.ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION_SPSPPS)
                        CopySpsPps((mfxExtCodingOptionSPSPPS *)d, (mfxExtCodingOptionSPSPPS *)src.ExtParam[i]);
                    else if (src.ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION_VPS)
                        CopyVps((mfxExtCodingOptionVPS *)d, (mfxExtCodingOptionVPS*)src.ExtParam[i]);
                    else
                        memmove(d, src.ExtParam[i], src.ExtParam[i]->BufferSz);
                }
            }
        }
    }

    int32_t CheckParamHasOnlyTemporalLayerChange(const mfxVideoParam &src, const mfxVideoParam &ref)
    {
        mfxVideoParam       mfxParam;
        AV1Enc::ExtBuffers extBuffers;
        Zero(mfxParam);
        extBuffers.CleanUp();
        mfxParam.NumExtParam = sizeof(extBuffers.extParamAll) / sizeof(extBuffers.extParamAll[0]);
        mfxParam.ExtParam = extBuffers.extParamAll;
        CopyParam(mfxParam, src);
        bool noChange = memcmp(&mfxParam.mfx, &ref.mfx, sizeof(mfxInfoMFX)) == 0;
        if (noChange && src.NumExtParam) {
            // Check others
            if (src.NumExtParam > ref.NumExtParam) noChange = 0;    // something more
            for (uint32_t i = 0; i < ref.NumExtParam && noChange; i++) {
                mfxExtBuffer *d = GetExtBufferById(src.ExtParam, src.NumExtParam, ref.ExtParam[i]->BufferId);
                if (d && d->BufferSz == ref.ExtParam[i]->BufferSz) {
                    if (d->BufferId == MFX_EXTBUFF_HEVC_TEMPORAL_LAYERS) {
                        // Allows Temporal Layers to change
                        mfxExtBuffer *c = GetExtBufferById(mfxParam.ExtParam, mfxParam.NumExtParam, ref.ExtParam[i]->BufferId);
                        mfxExtAvcTemporalLayers *tlayers = (mfxExtAvcTemporalLayers*)c;
                        mfxExtAvcTemporalLayers *tlayersref = (mfxExtAvcTemporalLayers*)ref.ExtParam[i];
                        if (tlayersref->Layer[0].Scale == 1 && tlayers->Layer[0].Scale == 1)
                            noChange = 1; // Both Setup
                        else
                            noChange = 0; // Not Setup
                    }
                    else {
                        noChange = memcmp(d, ref.ExtParam[i], d->BufferSz) == 0;
                    }
                }
            }
        }
        return noChange;
    }

    const struct LevelLimits {
        int32_t levelId;
        int32_t maxLumaPs;
        uint32_t maxCPB[2];    // main/high tier
        int32_t maxSliceSegmentsPerPicture;
        int32_t maxTileRows;
        int32_t maxTileCols;
        uint32_t maxLumaSr;
        int32_t maxBr[2];     // main/high tier
        int32_t minCrBase[2]; // main/high tier
    } tab_levelLimits[] = {
        { MFX_LEVEL_HEVC_1 ,    36864, {   350,      0},  16,  1,  1,     552960, {   128,      0}, {2, 2} },
        { MFX_LEVEL_HEVC_2 ,   122880, {  1500,      0},  16,  1,  1,    3686400, {  1500,      0}, {2, 2} },
        { MFX_LEVEL_HEVC_21,   245760, {  3000,      0},  20,  1,  1,    7372800, {  3000,      0}, {2, 2} },
        { MFX_LEVEL_HEVC_3 ,   552960, {  6000,      0},  30,  2,  2,   16588800, {  6000,      0}, {2, 2} },
        { MFX_LEVEL_HEVC_31,   983040, { 10000,      0},  40,  3,  3,   33177600, { 10000,      0}, {2, 2} },
        { MFX_LEVEL_HEVC_4 ,  2228224, { 12000,  30000},  75,  5,  5,   66846720, { 12000,  30000}, {4, 4} },
        { MFX_LEVEL_HEVC_41,  2228224, { 20000,  50000},  75,  5,  5,  133693440, { 20000,  50000}, {4, 4} },
        { MFX_LEVEL_HEVC_5 ,  8912896, { 25000, 100000}, 200, 11, 10,  267386880, { 25000, 100000}, {6, 4} },
        { MFX_LEVEL_HEVC_51,  8912896, { 40000, 160000}, 200, 11, 10,  534773760, { 40000, 160000}, {8, 4} },
        { MFX_LEVEL_HEVC_52,  8912896, { 60000, 240000}, 200, 11, 10, 1069547520, { 60000, 240000}, {8, 4} },
        { MFX_LEVEL_HEVC_6 , 35651584, { 60000, 240000}, 600, 22, 20, 1069547520, { 60000, 240000}, {8, 4} },
        { MFX_LEVEL_HEVC_61, 35651584, {120000, 480000}, 600, 22, 20, 2139095040, {120000, 480000}, {8, 4} },
        { MFX_LEVEL_HEVC_62, 35651584, {240000, 800000}, 600, 22, 20, 4278190080, {240000, 800000}, {6, 4} }
    };

    const int32_t NUM_LEVELS = sizeof(tab_levelLimits) / sizeof(tab_levelLimits[0]);

    int32_t Level(mfxU16 codecLevel) { return codecLevel & 0xff; }
    int32_t Tier(mfxU16 codecLevel) { return codecLevel & MFX_TIER_HEVC_HIGH; }

    int32_t GetBpp(uint32_t fourcc)
    {
        switch (fourcc) {
        case NV12: return 12;
        case NV16: return 16;
        case P010: return 15;
        case P210: return 20;
        default: assert(!"GetBpp: bad fourcc"); return 0;
        }
    }

    int32_t GetProfile(uint32_t fourcc)
    {
        switch (fourcc) {
        case NV12: return MAIN;
        case NV16: return REXT;
        case P010: return MAIN10;
        case P210: return REXT;
        default: assert(!"GetProfile: bad fourcc"); return 0;
        }
    }

    int32_t GetCpbNalFactor(int32_t profile)
    {
        switch (profile) {
        case MAIN:   return 1100;
        case MAIN10: return 1100;
        case REXT:   return 1833; // meaning Main 4:2:2 10 here as the only supported REXT profile
        default: assert(!"GetCpbVclFactor: bad profile"); return 0;
        }
    }

    uint16_t GetMinLevelForLumaPs(int32_t lumaPs)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].maxLumaPs >= lumaPs)
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint16_t GetMinLevelForLumaSr(int32_t lumaPs, uint32_t frNom, uint32_t frDenom)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].maxLumaSr * (uint64_t)frDenom >= lumaPs * (uint64_t)frNom)
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint16_t GetLevelIdx(uint32_t codecLevel)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].levelId == int32_t(codecLevel & 0xff))
                return (uint16_t)i;
        return NUM_LEVELS;
    }

    uint8_t GetMaxDpbSize(int32_t lumaPs, int32_t levelIdx)
    {
        const int32_t maxLumaPs = tab_levelLimits[levelIdx].maxLumaPs;
        const int32_t maxDpbPicBuf = 6;
        if      (lumaPs <= (maxLumaPs >> 2))     return MIN(4 * maxDpbPicBuf, 16);
        else if (lumaPs <= (maxLumaPs >> 1))     return MIN(2 * maxDpbPicBuf, 16);
        else if (lumaPs <= (3 * maxLumaPs >> 2)) return MIN(4 * maxDpbPicBuf / 3, 16);
        else                                     return maxDpbPicBuf;
    }

    uint16_t GetMinLevelForNumRefFrame(int32_t lumaPs, int32_t numRefFrame)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (numRefFrame <= GetMaxDpbSize(lumaPs, i))
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint16_t GetMinLevelForNumSlice(int32_t numSlice)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].maxSliceSegmentsPerPicture >= numSlice)
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint16_t GetMinLevelForNumTileRows(int32_t numTileRows)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].maxTileRows >= numTileRows)
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint16_t GetMinLevelForNumTileCols(int32_t numTileCols)
    {
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].maxTileCols >= numTileCols)
                return (uint16_t)tab_levelLimits[i].levelId;
        return 0xff;
    }

    uint32_t GetMaxBrForLevel(int32_t profile, uint16_t tierLevel)
    {
        const int32_t HbrFactor = 1;
        const int32_t CpbVclFactor = GetCpbNalFactor(profile);
        const int32_t BrNalFactor = CpbVclFactor * HbrFactor;
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].levelId == Level(tierLevel))
                return tab_levelLimits[i].maxBr[Tier(tierLevel) >> 8] * BrNalFactor;
        return 0;
    }

    uint16_t GetMinTierLevelForBitrateInKbps(int32_t profile, uint32_t bitrate)
    {
        const int32_t HbrFactor = 1;
        const int32_t CpbNalFactor = GetCpbNalFactor(profile);
        const int32_t BrNalFactor = CpbNalFactor * HbrFactor;
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            for (int32_t t = 0; t <= 1; t++)
                if (uint32_t(tab_levelLimits[i].maxBr[t] * BrNalFactor / 1000) >= bitrate)
                    return (uint16_t)(tab_levelLimits[i].levelId | (t << 8));
        return 0xff;
    }

    uint32_t GetMaxCpbForLevel(int32_t profile, uint16_t tierLevel)
    {
        const int32_t CpbNalFactor = GetCpbNalFactor(profile);
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            if (tab_levelLimits[i].levelId == Level(tierLevel))
                return tab_levelLimits[i].maxCPB[Tier(tierLevel) >> 8] * CpbNalFactor;
        return 0;
    }

    uint16_t GetMinTierLevelForBufferSizeInKB(int32_t profile, uint32_t bufferSizeInKB)
    {
        const int32_t CpbNalFactor = GetCpbNalFactor(profile);
        for (int32_t i = 0; i < NUM_LEVELS; i++)
            for (int32_t t = 0; t <= 1; t++)
                if (uint32_t(tab_levelLimits[i].maxCPB[t] * CpbNalFactor / 8000) >= bufferSizeInKB)
                    return (uint16_t)(tab_levelLimits[i].levelId | (t << 8));
        return 0xff;
    }

    template <class T, class U> bool CheckEq(T &v, U to)         { return (v && v != static_cast<T>(to))  ? (v = 0,                    false) : true; }
    template <class T>          bool CheckEq(T &v, uint32_t to)  { return CheckEq<T, uint32_t>(v, to); }
    template <class T>          bool CheckTriState(T &v)         { return (v && v != ON && v != OFF)      ? (v = 0,                    false) : true; }
    template <class T>          bool CheckTriStateAdaptive(T &v) { return (v && v != ON && v != OFF && v != ADAPT) ? (v = 0,           false) : true; }
    template <class T, class U> bool CheckMin(T &v, U minv)      { return (v && v < static_cast<T>(minv)) ? (v = 0,                    false) : true; }
    template <class T, class U> bool CheckMinSat(T &v, U minv)   { return (v && v < static_cast<T>(minv)) ? (v = static_cast<T>(minv), false) : true; }
    template <class T, class U> bool CheckMax(T &v, U maxv)      { return (v > static_cast<T>(maxv))      ? (v = 0,                    false) : true; }
    template <class T, class U> bool CheckMaxSat(T &v, U maxv)   { return (v > static_cast<T>(maxv))      ? (v = static_cast<T>(maxv), false) : true; }

    template <class T, class U> bool CheckRange(T &v, U minv, U maxv)    { assert(minv <= maxv); return CheckMax(v, maxv)    && CheckMin(v, minv); }
    template <class T, class U> bool CheckRangeSat(T &v, U minv, U maxv) { assert(minv <= maxv); return CheckMaxSat(v, maxv) && CheckMinSat(v, minv); }

    template <class U, size_t N, class T> bool IsIn(U (& arr)[N], const T &v) { return std::find(arr, arr + N, v) != arr + N; }
    template <class T, class U, size_t N> bool CheckSet(T &v, U (&setv)[N])   { return (v && !IsIn(setv, v)) ? (v = 0, false) : true; }

    uint16_t CheckMinLevel(uint16_t &tierLevel, uint16_t minLevel) {
        if (Level(tierLevel) < minLevel)
            return tierLevel = (uint16_t)(Tier(tierLevel) | minLevel), false; // bad, increased
        return true; // good
    }

    uint16_t CheckMinTierLevel(uint16_t &tierLevel, uint16_t minTierLevel) {
        uint16_t oldTierLevel = tierLevel;
        tierLevel = (uint16_t)(MAX(Level(tierLevel), Level(minTierLevel)) | MAX(Tier(tierLevel), Tier(minTierLevel)));
        return oldTierLevel == tierLevel;
    }

    bool IsCbrOrVbr(int32_t rateControlMethod) {
        return rateControlMethod == CBR || rateControlMethod == VBR;
    }
    bool IsCbrOrVbrOrAvbr(int32_t rateControlMethod) {
        return rateControlMethod == CBR || rateControlMethod == VBR || rateControlMethod == AVBR;
    }


    void MarkConfigurableParams(mfxVideoParam *out)
    {
        Zero(out->mfx);
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.CodecId = MFX_CODEC_HEVC; // restore cleared mandatory
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.IdrInterval = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.BRCParamMultiplier = 1;
        out->mfx.InitialDelayInKB = 1;
        out->mfx.BufferSizeInKB = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.MaxKbps = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumRefFrame = 1;
        out->mfx.EncodedOrder = 1;
        out->AsyncDepth = 1;
        out->IOPattern = 1;
        out->Protected = 0;

        if (mfxExtHEVCTiles *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->NumTileColumns = 1;
            ext->NumTileRows = 1;
        }

        if (mfxExtHEVCParam *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->PicWidthInLumaSamples = 1;
            ext->PicHeightInLumaSamples = 1;
            ext->GeneralConstraintFlags = 1;
        }

        if (mfxExtHEVCRegion *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->RegionEncoding = 1;
            ext->RegionType = 1;
            ext->RegionId = 1;
        }

        if (mfxExtCodingOptionAV1E *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->Log2MaxCUSize = 1;
            ext->MaxCUDepth = 1;
            ext->QuadtreeTULog2MaxSize = 1;
            ext->QuadtreeTULog2MinSize = 1;
            ext->QuadtreeTUMaxDepthIntra = 1;
            ext->QuadtreeTUMaxDepthInter = 1;
            ext->QuadtreeTUMaxDepthInterRD = 1;
            ext->AnalyzeChroma = 1;
            ext->SignBitHiding = 1;
            ext->RDOQuant = 1;
            ext->SAO      = 1;
            ext->SplitThresholdStrengthCUIntra = 1;
            ext->SplitThresholdStrengthTUIntra = 1;
            ext->SplitThresholdStrengthCUInter = 1;
            ext->SplitThresholdTabIndex        = 1;
            ext->SplitThresholdMultiplier = 1;
            ext->IntraNumCand1_2 = 1;
            ext->IntraNumCand1_3 = 1;
            ext->IntraNumCand1_4 = 1;
            ext->IntraNumCand1_5 = 1;
            ext->IntraNumCand1_6 = 1;
            ext->IntraNumCand2_2 = 1;
            ext->IntraNumCand2_3 = 1;
            ext->IntraNumCand2_4 = 1;
            ext->IntraNumCand2_5 = 1;
            ext->IntraNumCand2_6 = 1;
            ext->WPP = 1;
            ext->PartModes = 1;
            ext->CmIntraThreshold = 0;
            ext->TUSplitIntra = 1;
            ext->CUSplit = 1;
            ext->IntraAngModes = 1;
            ext->EnableCm = 0;
            ext->EnableCmInterp = 1;
            ext->BPyramid = 1;
            ext->FastPUDecision = 1;
            ext->HadamardMe = 1;
            ext->TMVP = 1;
            ext->Deblocking = 1;
            ext->RDOQuantChroma = 1;
            ext->RDOQuantCGZ = 1;
            ext->SaoOpt = 1;
            ext->SaoSubOpt = 1;
            ext->IntraNumCand0_2 = 1;
            ext->IntraNumCand0_3 = 1;
            ext->IntraNumCand0_4 = 1;
            ext->IntraNumCand0_5 = 1;
            ext->IntraNumCand0_6 = 1;
            ext->CostChroma = 1;
            ext->PatternIntPel = 1;
            ext->FastSkip = 1;
            ext->PatternSubPel = 1;
            ext->ForceNumThread = 1;
            ext->IntraChromaRDO = 1;
            ext->FastInterp = 1;
            ext->FastCbfMode = 1;
            ext->PuDecisionSatd = 1;
            ext->MinCUDepthAdapt = 1;
            ext->MaxCUDepthAdapt = 1;
            ext->NumBiRefineIter = 1;
            ext->CUSplitThreshold = 1;
            ext->DeltaQpMode = 1;
            ext->Enable10bit = 1;
            ext->IntraAngModesP = 1;
            ext->IntraAngModesBRef = 1;
            ext->IntraAngModesBnonRef = 1;
            ext->CpuFeature = 1;
            ext->TryIntra   = 1;
            ext->FastAMPSkipME = 1;
            ext->FastAMPRD = 1;
            ext->SkipMotionPartition = 1;
            ext->SkipCandRD = 1;
            ext->FramesInParallel = 1;
            ext->AdaptiveRefs = 1;
            ext->FastCoeffCost = 1;
            ext->NumRefFrameB = 1;
            ext->IntraMinDepthSC = 1;
            ext->InterMinDepthSTC = 1;
            ext->MotionPartitionDepth = 1;
            ext->AnalyzeCmplx = 1;
            ext->RateControlDepth = 1;
            ext->LowresFactor = 1;
            ext->DeblockBorders = 1;
            ext->SAOChroma = 1;
            ext->RepackProb = 1;
            ext->NumRefLayers = 1;
            ext->ConstQpOffset = 1;
#ifdef AMT_MAX_FRAME_SIZE
            ext->RepackForMaxFrameSize = 1;
#else
            ext->RepackForMaxFrameSize = 0;
#endif
            ext->FwdProbUpdateCoef = 1;
            ext->FwdProbUpdateSyntax = 1;
            ext->DeblockingLevelMethod = 1;
            ext->AllowHpMv = 1;
            ext->MaxTxDepthIntra = 1;
            ext->MaxTxDepthInter = 1;
            ext->MaxTxDepthIntraRefine = 1;
            ext->MaxTxDepthInterRefine = 1;
            ext->ChromaRDO = 1;
            ext->InterpFilter = 1;
            ext->InterpFilterRefine = 1;
            ext->IntraRDO = 1;
            ext->InterRDO = 1;
            ext->IntraInterRDO = 1;
            ext->CodecTypeExt = 1;
            ext->CDEF = 1;
            ext->LRMode = 1;
            ext->SRMode = 1;
            ext->CFLMode = 1;
            ext->ScreenMode = 1;
            ext->DisableFrameEndUpdateCdf = 1;
        }

        if (mfxExtCodingOption *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->RateDistortionOpt    = 0;
            ext->MECostType           = 0;
            ext->MESearchType         = 0;
            ext->MVSearchWindow.x     = 0;
            ext->MVSearchWindow.y     = 0;
            ext->EndOfSequence        = 0;
            ext->FramePicture         = 0;
            ext->CAVLC                = 0;
            ext->RecoveryPointSEI     = 0;
            ext->ViewOutput           = 0;
            ext->NalHrdConformance    = 0;
            ext->SingleSeiNalUnit     = 0;
            ext->VuiVclHrdParameters  = 0;
            ext->RefPicListReordering = 0;
            ext->ResetRefList         = 0;
            ext->RefPicMarkRep        = 0;
            ext->FieldOutput          = 0;
            ext->IntraPredBlockSize   = 0;
            ext->InterPredBlockSize   = 0;
            ext->MVPrecision          = 0;
            ext->MaxDecFrameBuffering = 0;
            ext->AUDelimiter          = 1;
            ext->EndOfStream          = 0;
            ext->PicTimingSEI         = 0;
            ext->VuiNalHrdParameters  = 0;
        }

        if (mfxExtCodingOption2 *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->IntRefType = 0;
            ext->IntRefCycleSize = 0;
            ext->IntRefQPDelta = 0;
            ext->MaxFrameSize = 1;
            ext->MaxSliceSize = 0;
            ext->BitrateLimit = 0;
            ext->MBBRC = 1;
            ext->ExtBRC = 0;
            ext->LookAheadDepth = 0;
            ext->Trellis = 0;
            ext->RepeatPPS = 0;
            ext->BRefType = 0;
            ext->AdaptiveI = 1;
            ext->AdaptiveB = 0;
            ext->LookAheadDS = 0;
            ext->NumMbPerSlice = 0;
            ext->SkipFrame = 0;
            ext->MinQPI = 0;
            ext->MaxQPI = 0;
            ext->MinQPP = 0;
            ext->MaxQPP = 0;
            ext->MinQPB = 0;
            ext->MaxQPB = 0;
            ext->FixedFrameRate = 0;
            ext->DisableDeblockingIdc = 0;
            ext->DisableVUI = 1;
            ext->BufferingPeriodSEI = 0;
            ext->EnableMAD = 0;
            ext->UseRawRef = 0;
        }

        if (mfxExtCodingOption3 *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->NumSliceI = 0;
            ext->NumSliceP = 0;
            ext->NumSliceB = 0;
            ext->WinBRCMaxAvgKbps = 0;
            ext->WinBRCSize = 0;
            ext->QVBRQuality = 0;
            ext->EnableMBQP = 1;
            ext->IntRefCycleDist = 0;
            ext->DirectBiasAdjustment = 0;
            ext->GlobalMotionBiasAdjustment = 0;
            ext->MVCostScalingFactor = 0;
            ext->MBDisableSkipMap = 0;
            ext->WeightedPred = 0;
            ext->WeightedBiPred = 0;
            ext->AspectRatioInfoPresent = 0;
            ext->OverscanInfoPresent = 0;
            ext->OverscanAppropriate = 0;
            ext->TimingInfoPresent = 0;
            ext->BitstreamRestriction = 0;
            ext->LowDelayHrd = 0;
            ext->MotionVectorsOverPicBoundaries = 0;
            ext->ScenarioInfo = 1;
            ext->ContentInfo = 0;
            ext->PRefType = 1;
            ext->FadeDetection = 0;
            ext->GPB = 1;
            ext->ExtBrcAdaptiveLTR = 1;
        }

        if (mfxExtEncoderROI *ext = GetExtBuffer(*out)) {
            InitExtBuffer0(*ext);
            ext->NumROI = 1;
            for (int32_t i = 0; i < 256; i++) {
                ext->ROI[i].Left = ext->ROI[i].Right = ext->ROI[i].Top = ext->ROI[i].Bottom = 1;
                ext->ROI[i].Priority = 1;
            }
        }

        if (mfxExtAVCRefListCtrl * ctrl = GetExtBuffer(*out))
        {
            mfxExtBuffer tmp = ctrl->Header;
            Zero(*ctrl);
            ctrl->Header = tmp;

            ctrl->NumRefIdxL0Active = 1;
            ctrl->NumRefIdxL1Active = 0;
            ctrl->ApplyLongTermIdx = 0;

            ctrl->LongTermRefList[0].FrameOrder = 1;
            ctrl->LongTermRefList[0].LongTermIdx = 0;
            ctrl->PreferredRefList[0].FrameOrder = 1;
            ctrl->RejectedRefList[0].FrameOrder = 1;
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        if (mfxExtAV1Param * ctrl = GetExtBuffer(*out))
        {
            mfxExtBuffer tmp = ctrl->Header;
            Zero(*ctrl);
            ctrl->Header = tmp;

            ctrl->FrameHeight = 0;
            ctrl->FrameWidth = 0;
            ctrl->NumTileColumns = 0;
            ctrl->NumTileRows = 0;
            ctrl->WriteIVFHeaders = 1;
            ctrl->DisableCdfUpdate = 1;
            ctrl->DisableFrameEndUpdateCdf = 1;
            ctrl->EnableCdef = 1;
            ctrl->EnableRestoration = 1;
            ctrl->EnableSuperres = 1;
            ctrl->InsertTemporalDelimiter = 0;
            ctrl->InterpFilter = 1;
            ctrl->LoopFilterSharpness = 0;
            ctrl->NumTileGroups = 0;
            ctrl->PackOBUFrame = 0;
            ctrl->SegmentationMode = 0;
            ctrl->StillPictureMode = 0;
            ctrl->SuperresScaleDenominator = 0;
            ctrl->SwitchInterval = 0;
            ctrl->UseAnnexB = 0;
        }
#endif
        if (mfxExtVP9Param * ctrl = GetExtBuffer(*out))
        {
            mfxExtBuffer tmp = ctrl->Header;
            Zero(*ctrl);
            ctrl->Header = tmp;

            ctrl->WriteIVFHeaders = 1;
        }
    }

    struct ErrorFlag
    {
        ErrorFlag(bool f = false) : flag(f) {}
        operator bool() const { return flag; }
        void operator =(bool f) {
            if (f)
                flag = true; // set BREAKPOINT here
        }
    private:
        bool flag;
    };

    mfxStatus CheckParam(mfxVideoParam &par)
    {
        ErrorFlag errUnsupported = false;
        ErrorFlag errInvalidParam = false;
        ErrorFlag wrnIncompatible = false;

        mfxInfoMFX &mfx = par.mfx;
        mfxFrameInfo &fi = mfx.FrameInfo;
        mfxExtCodingOptionAV1E *optHevc = GetExtBuffer(par);
        mfxExtHEVCRegion *region = GetExtBuffer(par);
        //mfxExtHEVCTiles *tiles = GetExtBuffer(par);
        mfxExtHEVCParam *hevcParam = GetExtBuffer(par);
        mfxExtCodingOption *opt = GetExtBuffer(par);
        mfxExtCodingOption2 *opt2 = GetExtBuffer(par);
        mfxExtCodingOption3 *opt3 = GetExtBuffer(par);
        mfxExtDumpFiles *dumpFiles = GetExtBuffer(par);
        mfxExtEncoderROI *roi = GetExtBuffer(par);
        mfxExtAvcTemporalLayers *tlayers = GetExtBuffer(par);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        mfxExtAV1Param *av1param = GetExtBuffer(par);
#endif

        // check pairs (error is expected behavior)
        if (!fi.FrameRateExtN != !fi.FrameRateExtD)
            fi.FrameRateExtN = fi.FrameRateExtD = 0, errUnsupported = true;
        if (!fi.FourCC != !fi.ChromaFormat)
            fi.FourCC = fi.ChromaFormat = 0, errUnsupported = true;
        if (!fi.AspectRatioW != !fi.AspectRatioH)
            fi.AspectRatioW = fi.AspectRatioH = 0, wrnIncompatible = true;

        // check min/max/supported
        errUnsupported = !CheckEq(par.Protected, 0);
        errUnsupported = !CheckEq(mfx.CodecId, (uint32_t)MFX_CODEC_AV1);
        errUnsupported = !CheckSet(fi.ChromaFormat, CodecLimits::SUP_CHROMA_FORMAT);
        errUnsupported = !CheckMax(fi.Width, CodecLimits::MAX_WIDTH);
        errUnsupported = !CheckMax(fi.Height, CodecLimits::MAX_HEIGHT);
        errUnsupported = !CheckMin(fi.Width, CodecLimits::MIN_WIDTH_IMPL);
        errUnsupported = !CheckMin(fi.Height, CodecLimits::MIN_HEIGHT_IMPL);
        errInvalidParam = !CheckSet(fi.PicStruct, CodecLimits::SUP_PIC_STRUCT);
        if(fi.PicStruct == TFF || fi.PicStruct == BFF)
            fi.PicStruct = PROGR, wrnIncompatible = true;

        errInvalidParam = !CheckSet(mfx.RateControlMethod, CodecLimits::SUP_RC_METHOD);
        errInvalidParam = !CheckSet(fi.FourCC, CodecLimits::SUP_FOURCC);
        errInvalidParam = !CheckMax(mfx.NumSlice, CodecLimits::MAX_NUM_SLICE);
        errInvalidParam = !CheckMax(fi.CropX, CodecLimits::MAX_WIDTH - 1);
        errInvalidParam = !CheckMax(fi.CropY, CodecLimits::MAX_HEIGHT - 1);
        errInvalidParam = !CheckMax(fi.CropW, CodecLimits::MAX_WIDTH);
        errInvalidParam = !CheckMax(fi.CropH, CodecLimits::MAX_HEIGHT);
        if (fi.Width & 15)
            fi.Width = 0, errInvalidParam = true;
        if (fi.Height & 15)
            fi.Height = 0, errInvalidParam = true;

        wrnIncompatible = !CheckSet(mfx.GopOptFlag, CodecLimits::SUP_GOP_OPT_FLAG);
        wrnIncompatible = !CheckSet(mfx.CodecProfile, CodecLimits::SUP_PROFILE);
        wrnIncompatible = !CheckSet(mfx.CodecLevel, CodecLimits::SUP_LEVEL);
        wrnIncompatible = !CheckRangeSat(mfx.TargetUsage, CodecLimits::MIN_TARGET_USAGE, CodecLimits::MAX_TARGET_USAGE);
        wrnIncompatible = !CheckMaxSat(mfx.GopRefDist, CodecLimits::MAX_GOP_REF_DIST);
        wrnIncompatible = !CheckMaxSat(mfx.NumRefFrame, 8);
        wrnIncompatible = !CheckMinSat(mfx.NumRefFrame, 2);

        if (mfx.RateControlMethod == CQP) {
            wrnIncompatible = !CheckMaxSat(mfx.QPI, 255);
            wrnIncompatible = !CheckMaxSat(mfx.QPP, 255);
            wrnIncompatible = !CheckMaxSat(mfx.QPB, 255);
        } else {
            int32_t brcMultiplier = MAX(1, mfx.BRCParamMultiplier);
            int32_t maxBufferInKB = MIN(0xffff, GetMaxCpbForLevel(REXT, H62) / 8000 / brcMultiplier);
            int32_t maxRateKbps   = MIN(0xffff, GetMaxBrForLevel(REXT, H62) / 1000 / brcMultiplier);

            wrnIncompatible = !CheckMaxSat(mfx.TargetKbps, maxRateKbps);
            if (mfx.RateControlMethod == VBR)
                wrnIncompatible = !CheckMaxSat(mfx.MaxKbps, maxRateKbps);
            if (IsCbrOrVbr(mfx.RateControlMethod)) {
                wrnIncompatible = !CheckMaxSat(mfx.BufferSizeInKB, maxBufferInKB);
                wrnIncompatible = !CheckMaxSat(mfx.InitialDelayInKB, maxBufferInKB);
            }
        }

        if (par.IOPattern != 0 && par.IOPattern != SYSMEM &&
            par.IOPattern != VIDMEM && par.IOPattern != OPAQMEM) {
            if (par.IOPattern & SYSMEM)
                par.IOPattern = SYSMEM, wrnIncompatible = true;
            else if (par.IOPattern & VIDMEM)
                par.IOPattern = VIDMEM, wrnIncompatible = true;
            else if (par.IOPattern & OPAQMEM)
                par.IOPattern = OPAQMEM, wrnIncompatible = true;
            else
                par.IOPattern = 0, errInvalidParam = true;
        }

        //if (tiles) {
        //    if (optHevc) {
        //        wrnIncompatible = !CheckMaxSat(tiles->NumTileRows, CodecLimits::AV1_MAX_NUM_TILE_ROWS);
        //        wrnIncompatible = !CheckMaxSat(tiles->NumTileColumns, CodecLimits::AV1_MAX_NUM_TILE_COLS);
        //    }
        //}

        if (region) {
            if (region->RegionEncoding != MFX_HEVC_REGION_ENCODING_ON && region->RegionEncoding != MFX_HEVC_REGION_ENCODING_OFF)
                region->RegionEncoding = MFX_HEVC_REGION_ENCODING_OFF, wrnIncompatible = true;
            if (region->RegionEncoding == MFX_HEVC_REGION_ENCODING_ON) {
                errUnsupported = !CheckEq(region->RegionType, (uint32_t)MFX_HEVC_REGION_SLICE);
                errUnsupported = !CheckMax(region->RegionId, CodecLimits::MAX_NUM_SLICE - 1);
            }
        }

        if (hevcParam) {
            wrnIncompatible = !CheckEq(hevcParam->GeneralConstraintFlags, MAIN_422_10);
            errUnsupported = !CheckMax(hevcParam->PicWidthInLumaSamples, CodecLimits::MAX_WIDTH);
            errUnsupported = !CheckMax(hevcParam->PicHeightInLumaSamples, CodecLimits::MAX_HEIGHT);
            if (hevcParam->PicWidthInLumaSamples & 7)
                hevcParam->PicWidthInLumaSamples = 0, errInvalidParam = true;
            if (hevcParam->PicHeightInLumaSamples & 7)
                hevcParam->PicHeightInLumaSamples = 0, errInvalidParam = true;
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        if (av1param) {
            wrnIncompatible = !CheckTriState(av1param->EnableCdef);
            wrnIncompatible = !CheckTriState(av1param->EnableRestoration);
            wrnIncompatible = !CheckTriState(av1param->DisableFrameEndUpdateCdf);
            wrnIncompatible = !CheckTriState(av1param->EnableSuperres);
            wrnIncompatible = !CheckMax(av1param->InterpFilter, MFX_AV1_INTERP_SWITCHABLE);
        }
#endif
        if (optHevc) {
            wrnIncompatible = !CheckRange(optHevc->Log2MaxCUSize, 5, 6);
            wrnIncompatible = !CheckRange(optHevc->QuadtreeTULog2MaxSize, 2, 5);
            wrnIncompatible = !CheckRange(optHevc->QuadtreeTULog2MinSize, 2, 5);

            wrnIncompatible = !CheckMax(optHevc->MaxCUDepth, 4);
            wrnIncompatible = !CheckMax(optHevc->QuadtreeTUMaxDepthIntra, 4);
            wrnIncompatible = !CheckMax(optHevc->QuadtreeTUMaxDepthInter, 4);
            wrnIncompatible = !CheckMax(optHevc->QuadtreeTUMaxDepthInterRD, 4);
            wrnIncompatible = !CheckMax(optHevc->SplitThresholdStrengthCUIntra, 3);
            wrnIncompatible = !CheckMax(optHevc->SplitThresholdStrengthTUIntra, 3);
            wrnIncompatible = !CheckMax(optHevc->SplitThresholdStrengthCUInter, 3);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand0_2, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand0_3, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand0_4, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand0_5, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand0_6, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand1_2, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand1_3, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand1_4, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand1_5, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand1_6, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand2_2, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand2_3, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand2_4, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand2_5, 35);
            wrnIncompatible = !CheckMax(optHevc->IntraNumCand2_6, 35);
            wrnIncompatible = !CheckMax(optHevc->PartModes, 3);
            wrnIncompatible = !CheckMax(optHevc->TUSplitIntra, 3);
            wrnIncompatible = !CheckMax(optHevc->CUSplit, 2);
            wrnIncompatible = !CheckMax(optHevc->HadamardMe, 3);
            wrnIncompatible = !CheckMax(optHevc->SaoOpt, 3);
            wrnIncompatible = !CheckMax(optHevc->SaoSubOpt, 3);
            wrnIncompatible = !CheckMax(optHevc->CpuFeature, 5);
            wrnIncompatible = !CheckMax(optHevc->TryIntra, 2);
            wrnIncompatible = !CheckMax(optHevc->SplitThresholdTabIndex, 3);
            wrnIncompatible = !CheckMax(optHevc->FastAMPSkipME, 2);
            wrnIncompatible = !CheckMax(optHevc->FastAMPRD, 2);
            wrnIncompatible = !CheckMax(optHevc->SkipMotionPartition, 2);
            wrnIncompatible = !CheckMax(optHevc->PatternSubPel, 6);
#ifdef AMT_HROI_PSY_AQ
            wrnIncompatible = !CheckMax(optHevc->DeltaQpMode, 32);
#else
            wrnIncompatible = !CheckMax(optHevc->DeltaQpMode, 8);
#endif
            wrnIncompatible = !CheckMax(optHevc->NumRefFrameB, 16);
            wrnIncompatible = !CheckMax(optHevc->NumRefLayers, 4);
            wrnIncompatible = !CheckMax(optHevc->AnalyzeCmplx, 2);
            wrnIncompatible = !CheckMax(optHevc->LowresFactor, 3);
            wrnIncompatible = !CheckMax(optHevc->DeblockingLevelMethod, 3);
            wrnIncompatible = !CheckMax(optHevc->MaxTxDepthIntra, 4);
            wrnIncompatible = !CheckMax(optHevc->MaxTxDepthInter, 4);
            wrnIncompatible = !CheckMax(optHevc->MaxTxDepthIntraRefine, 4);
            wrnIncompatible = !CheckMax(optHevc->MaxTxDepthInterRefine, 4);
            wrnIncompatible = !CheckMax(optHevc->CodecTypeExt, 2);

            wrnIncompatible = !CheckTriState(optHevc->AnalyzeChroma);
            wrnIncompatible = !CheckTriState(optHevc->SignBitHiding);
            wrnIncompatible = !CheckTriState(optHevc->RDOQuant);
            wrnIncompatible = !CheckTriState(optHevc->SAO);
            wrnIncompatible = !CheckTriState(optHevc->WPP);
            wrnIncompatible = !CheckTriState(optHevc->BPyramid);
            wrnIncompatible = !CheckTriState(optHevc->FastPUDecision);
            wrnIncompatible = !CheckTriState(optHevc->TMVP);
            wrnIncompatible = !CheckTriState(optHevc->Deblocking);
            wrnIncompatible = !CheckTriState(optHevc->RDOQuantChroma);
            wrnIncompatible = !CheckTriState(optHevc->RDOQuantCGZ);
            wrnIncompatible = !CheckTriState(optHevc->CostChroma);
            wrnIncompatible = !CheckTriState(optHevc->FastSkip);
            wrnIncompatible = !CheckTriState(optHevc->FastCbfMode);
            wrnIncompatible = !CheckTriState(optHevc->PuDecisionSatd);
            wrnIncompatible = !CheckTriState(optHevc->MinCUDepthAdapt);
            wrnIncompatible = !CheckTriState(optHevc->MaxCUDepthAdapt);
            wrnIncompatible = !CheckTriState(optHevc->Enable10bit);
            wrnIncompatible = !CheckTriState(optHevc->IntraChromaRDO);
            wrnIncompatible = !CheckTriState(optHevc->FastInterp);
            wrnIncompatible = !CheckTriState(optHevc->SkipCandRD);
            wrnIncompatible = !CheckTriState(optHevc->AdaptiveRefs);
            wrnIncompatible = !CheckTriState(optHevc->FastCoeffCost);
            wrnIncompatible = !CheckTriState(optHevc->DeblockBorders);
            wrnIncompatible = !CheckTriState(optHevc->SAOChroma);
            wrnIncompatible = !CheckTriState(optHevc->AllowHpMv);
            wrnIncompatible = !CheckTriState(optHevc->ChromaRDO);
            wrnIncompatible = !CheckTriState(optHevc->InterpFilter);
            wrnIncompatible = !CheckTriState(optHevc->InterpFilterRefine);
            wrnIncompatible = !CheckTriState(optHevc->IntraRDO);
            wrnIncompatible = !CheckTriState(optHevc->InterRDO);
            wrnIncompatible = !CheckTriState(optHevc->IntraInterRDO);
            wrnIncompatible = !CheckTriState(optHevc->CDEF);
            wrnIncompatible = !CheckTriState(optHevc->LRMode);
            wrnIncompatible = !CheckTriState(optHevc->SRMode);
            wrnIncompatible = !CheckTriState(optHevc->CFLMode);

            wrnIncompatible = !CheckSet(optHevc->IntraAngModes, CodecLimits::SUP_INTRA_ANG_MODE_I);
            wrnIncompatible = !CheckSet(optHevc->EnableCm, CodecLimits::SUP_ENABLE_CM);
            wrnIncompatible = !CheckSet(optHevc->PatternIntPel, CodecLimits::SUP_PATTERN_INT_PEL);
            wrnIncompatible = !CheckSet(optHevc->IntraAngModesP, CodecLimits::SUP_INTRA_ANG_MODE);
            wrnIncompatible = !CheckSet(optHevc->IntraAngModesBRef, CodecLimits::SUP_INTRA_ANG_MODE);
            wrnIncompatible = !CheckSet(optHevc->IntraAngModesBnonRef, CodecLimits::SUP_INTRA_ANG_MODE);

            wrnIncompatible = !CheckTriState(optHevc->RepackForMaxFrameSize);
        }

        if (opt) {
            wrnIncompatible = !CheckTriState(opt->AUDelimiter);
        }

        if (opt2) {
            wrnIncompatible = !CheckTriState(opt2->AdaptiveI);
            wrnIncompatible = !CheckTriState(opt2->DisableVUI);
            wrnIncompatible = !CheckTriState(opt2->MBBRC);
        }

        if (opt3) {
            wrnIncompatible = !CheckTriState(opt3->GPB);
            wrnIncompatible = !CheckTriStateAdaptive(opt3->EnableMBQP);
            wrnIncompatible = !CheckTriState(opt3->ExtBrcAdaptiveLTR);
        }

        // check combinations

        if (fi.FrameRateExtN && fi.FrameRateExtD) // FR <= 300
            if (fi.FrameRateExtN > (uint64_t)300 * fi.FrameRateExtD)
                fi.FrameRateExtN = fi.FrameRateExtD = 0, errInvalidParam = true;

        if (fi.ChromaFormat && fi.FourCC) // FourCC & ChromaFormat
            if (fi.ChromaFormat == YUV420 && fi.FourCC != NV12 && fi.FourCC != P010 ||
                fi.ChromaFormat == YUV422 && fi.FourCC != NV16 && fi.FourCC != P210)
                fi.ChromaFormat = 0, fi.FourCC = 0, errUnsupported = true;

        if (hevcParam && fi.Width) // PicWidthInLumaSamples <= Width
            errInvalidParam = !CheckMax(hevcParam->PicWidthInLumaSamples, fi.Width);
        if (hevcParam && fi.Height) // PicHeightInLumaSamples <= Height
            errInvalidParam = !CheckMax(hevcParam->PicHeightInLumaSamples, fi.Height);

        uint16_t &picWidth  = hevcParam && hevcParam->PicWidthInLumaSamples  ? hevcParam->PicWidthInLumaSamples  : fi.Width;
        uint16_t &picHeight = hevcParam && hevcParam->PicHeightInLumaSamples ? hevcParam->PicHeightInLumaSamples : fi.Height;

        // Width, Height, ChromaFormat, CropXYWH
        if (fi.CropX & 1)
            fi.CropX--, wrnIncompatible = true;
        if (fi.CropW & 1)
            fi.CropW++, wrnIncompatible = true;
        if (fi.CropX && picWidth)
            errInvalidParam = !CheckMax(fi.CropX, picWidth - 1);
        if (fi.CropW && picWidth)
            errInvalidParam = !CheckMax(fi.CropW, picWidth);
        if (fi.CropX && fi.CropW && picWidth)
            errInvalidParam = !CheckMax(fi.CropX, picWidth - fi.CropW);
        if (fi.ChromaFormat == YUV420 && (fi.CropY & 1))
            fi.CropY--, wrnIncompatible = true;
        if (fi.ChromaFormat == YUV420 && (fi.CropH & 1))
            fi.CropH++, wrnIncompatible = true;
        if (fi.CropY && picHeight)
            errInvalidParam = !CheckMax(fi.CropY, picHeight - 1);
        if (fi.CropH && picHeight)
            errInvalidParam = !CheckMax(fi.CropH, picHeight);
        if (fi.CropY && fi.CropH && picHeight)
            errInvalidParam = !CheckMax(fi.CropY, picHeight - fi.CropH);


        if (roi && roi->NumROI) {
             wrnIncompatible = !CheckMaxSat(roi->NumROI, 256);
             for (int i = 0; i < roi->NumROI; i++) {
                 if (picWidth)
                     wrnIncompatible = !CheckMaxSat(roi->ROI[i].Right, picWidth);
                 else
                     errInvalidParam = !CheckMax(roi->ROI[i].Right, CodecLimits::MAX_WIDTH);
                 if (roi->ROI[i].Left > roi->ROI[i].Right)
                     roi->ROI[i].Left = roi->ROI[i].Right = 0, wrnIncompatible = true;
                 if (picHeight)
                     wrnIncompatible = !CheckMaxSat(roi->ROI[i].Bottom, picHeight);
                 else
                     errInvalidParam = !CheckMax(roi->ROI[i].Bottom, CodecLimits::MAX_HEIGHT);
                 if (roi->ROI[i].Top > roi->ROI[i].Bottom)
                     roi->ROI[i].Top = roi->ROI[i].Bottom = 0, wrnIncompatible = true;

                 //if (optHevc && optHevc->Log2MaxCUSize) {
                 //    uint32_t mask = (1 << optHevc->Log2MaxCUSize) - 1;
                 {
                     uint16_t mask = (1 << 6) - 1;
                     if (roi->ROI[i].Left & mask)
                         roi->ROI[i].Left &= ~mask, wrnIncompatible = true;
                     if (roi->ROI[i].Right & mask)
                         roi->ROI[i].Right &= ~mask, wrnIncompatible = true;
                     if (roi->ROI[i].Top & mask)
                         roi->ROI[i].Top &= ~mask, wrnIncompatible = true;
                     if (roi->ROI[i].Bottom & mask)
                         roi->ROI[i].Bottom &= ~mask, wrnIncompatible = true;
                 }

                 if (mfx.RateControlMethod == CQP) {
                     if (fi.FourCC) {
                         int32_t maxdqp = 255;
                         //if (fi.FourCC == P010 || fi.FourCC == P210)
                         //    maxdqp = 255;
                         wrnIncompatible = !CheckRangeSat(roi->ROI[i].Priority, -maxdqp, maxdqp);
                     }
                 }
                 else
                     wrnIncompatible = !CheckRangeSat(roi->ROI[i].Priority, -3 * DEFAULT_DELTA_Q_RES, 3 * DEFAULT_DELTA_Q_RES);
             }
             if (optHevc)
                wrnIncompatible = !CheckMaxSat(optHevc->DeltaQpMode, 1);
        }

        if (mfx.GopPicSize && mfx.GopRefDist) // GopRefDist <= GopPicSize
            wrnIncompatible = !CheckMaxSat(mfx.GopRefDist, mfx.GopPicSize);

        if (fi.FourCC && mfx.CodecProfile) { // FourCC and CodecProfile should match
            if (fi.FourCC == P010 && mfx.CodecProfile == MAIN) // P010 should be encoded as MAIN10 or REXT
                mfx.CodecProfile = MAIN10, wrnIncompatible = true;
            if ((fi.FourCC == NV16 || fi.FourCC == P210) && mfx.CodecProfile != REXT)  // 4:2:2 should be encoded as REXT
                mfx.CodecProfile = REXT, wrnIncompatible = true;
        }

        if ((fi.FourCC == NV16 || fi.FourCC == P210) && hevcParam && hevcParam->GeneralConstraintFlags) // NV16 and P210 should be encoded as "Main 4:2:2 10"
            if (hevcParam->GeneralConstraintFlags != MAIN_422_10)
                hevcParam->GeneralConstraintFlags = MAIN_422_10, wrnIncompatible = true;

        if (mfx.CodecProfile && hevcParam) {
            if (mfx.CodecProfile != REXT && hevcParam->GeneralConstraintFlags != 0) // should be no flags for MAIN and MAIN10
                hevcParam->GeneralConstraintFlags = 0, wrnIncompatible = true;
            if (mfx.CodecProfile == REXT) // REXT profile should be "Main 4:2:2 10"
                wrnIncompatible = !CheckEq(hevcParam->GeneralConstraintFlags, (uint64_t)MAIN_422_10);
        }

        if (picWidth && mfx.CodecLevel) // W <= sqrt(MaxLumaPs[Level] * 8)
            wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForLumaPs(picWidth * picWidth / 8));

        if (picHeight && mfx.CodecLevel) // H <= sqrt(MaxLumaPs[Level] * 8)
            wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForLumaPs(picHeight * picHeight / 8));

        if (picWidth && picHeight && mfx.CodecLevel) // W * H <= MaxLumaPs[Level]
            wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForLumaPs(picWidth * picHeight));

        if (picWidth && picHeight && fi.FrameRateExtN && fi.FrameRateExtD) // W * H * FR <= MaxLumaSr[High6.2]
            if (picWidth * picHeight * (uint64_t)fi.FrameRateExtN > (uint64_t)fi.FrameRateExtD * tab_levelLimits[NUM_LEVELS-1].maxLumaSr)
                picWidth = picHeight = 0, fi.FrameRateExtN = fi.FrameRateExtD = 0, errUnsupported = true;

        if (picWidth && picHeight && fi.FrameRateExtN && fi.FrameRateExtD && mfx.CodecLevel) // W * H * FR <= MaxLumaSr[Level]
            wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForLumaSr(picWidth * picHeight, fi.FrameRateExtN, fi.FrameRateExtD));

        if (picWidth && picHeight && mfx.NumRefFrame) // NumRefFrame <= MaxDpbSize(W, H, High62)
            wrnIncompatible = !CheckMaxSat(mfx.NumRefFrame, GetMaxDpbSize(picWidth * picHeight, NUM_LEVELS-1));

        if (picWidth && picHeight && mfx.NumRefFrame && mfx.CodecLevel) // NumRefFrame <= MaxDpbSize(W, H, Level)
            wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumRefFrame(picWidth * picHeight, mfx.NumRefFrame));

        if (mfx.NumRefFrame && optHevc && optHevc->NumRefFrameB) // NumRefFrameB <= NumRefFrame
            wrnIncompatible = !CheckMax(optHevc->NumRefFrameB, mfx.NumRefFrame);

        //if (mfx.NumSlice > 1 && tiles && (tiles->NumTileColumns > 1 || tiles->NumTileRows > 1)) // multi-slice or multi-tile
        //    mfx.NumSlice = 1, wrnIncompatible = true;

        //if (mfx.NumSlice > 1 && (fi.PicStruct == TFF || fi.PicStruct == BFF)) // multi-slice is unsupported for interlace
        //    mfx.NumSlice = 1, wrnIncompatible = true;

        //if (picHeight && mfx.NumSlice) // NumSlice < CTB lines for min possible CTBSize
        //    wrnIncompatible = !CheckMaxSat(mfx.NumSlice, (picHeight + 63) >> 6);

        //if (picHeight && mfx.NumSlice && optHevc && optHevc->Log2MaxCUSize) // NumSlice < CTB lines for given CTBSize
        //    wrnIncompatible = !CheckMaxSat(mfx.NumSlice, (picHeight + (1 << optHevc->Log2MaxCUSize) - 1) >> optHevc->Log2MaxCUSize);

        //if (mfx.NumSlice && mfx.CodecLevel) // NumSlice <= MaxNumSlice[level]
        //    wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumSlice(mfx.NumSlice));

        //if (tiles && (tiles->NumTileColumns > 1 || tiles->NumTileRows > 1) && (fi.PicStruct == TFF || fi.PicStruct == BFF)) // multi-tile is unsupported for interlace
        //    tiles->NumTileColumns = tiles->NumTileRows = 1, wrnIncompatible = true;

        //if (picWidth && tiles && tiles->NumTileColumns > 1) // ColumnWidthInLumaSamples[i] < 256
        //    wrnIncompatible = !CheckMaxSat(tiles->NumTileColumns, MAX(1, picWidth / CodecLimits::MIN_TILE_WIDTH));

        //if (picHeight && tiles && tiles->NumTileRows > 1) // RowHeightInLumaSamples[i] < 64
        //    wrnIncompatible = !CheckMaxSat(tiles->NumTileRows, MAX(1, picHeight / CodecLimits::MIN_TILE_HEIGHT));

        //if (tiles && tiles->NumTileRows > 1 && mfx.CodecLevel) // NumTileRows <= MaxNumTileRows[level]
        //    wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumTileRows(tiles->NumTileRows));

        //if (tiles && tiles->NumTileColumns > 1 && mfx.CodecLevel) // NumTileColumns <= MaxNumTileCols[level]
        //    wrnIncompatible = !CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumTileRows(tiles->NumTileColumns));

        // check BRC params
        uint32_t targetKbps       = MAX(1, mfx.BRCParamMultiplier) * mfx.TargetKbps;
        uint32_t maxKbps          = MAX(1, mfx.BRCParamMultiplier) * mfx.MaxKbps;
        uint32_t bufferSizeInKB   = MAX(1, mfx.BRCParamMultiplier) * mfx.BufferSizeInKB;
        uint32_t initialDelayInKB = MAX(1, mfx.BRCParamMultiplier) * mfx.InitialDelayInKB;
        const uint16_t profile = mfx.CodecProfile ? mfx.CodecProfile : fi.FourCC ? (uint16_t)GetProfile(fi.FourCC) : 0;

        // check targetKbps <= raw data rate
        if (mfx.RateControlMethod != CQP && picWidth && picHeight && fi.FourCC && fi.FrameRateExtN && fi.FrameRateExtD && targetKbps) {
            uint32_t rawDataKbps = (uint32_t)MIN(MAX_UINT, (uint64_t)picWidth * picHeight * GetBpp(fi.FourCC) * fi.FrameRateExtN / fi.FrameRateExtD / 1000);
            wrnIncompatible = !CheckMaxSat(targetKbps, rawDataKbps);
        }

        if (mfx.RateControlMethod == VBR && targetKbps && maxKbps) // maxKbps >= targetKbps
            wrnIncompatible = !CheckMinSat(maxKbps, targetKbps);

        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && targetKbps) // targetKbps <= MaxBR[High6.2]
            wrnIncompatible = !CheckMaxSat(targetKbps, GetMaxBrForLevel(profile, H62) / 1000);

        if (mfx.RateControlMethod == VBR && profile && maxKbps) // maxKbps <= MaxBR[High6.2]
            wrnIncompatible = !CheckMaxSat(maxKbps, GetMaxBrForLevel(profile, H62) / 1000);

        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && targetKbps && mfx.CodecLevel) // targetKbps <= MaxBR[tierLevel]
            if (targetKbps > GetMaxBrForLevel(profile, mfx.CodecLevel) / 1000)
                wrnIncompatible = !CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBitrateInKbps(profile, targetKbps));

        if (mfx.RateControlMethod == VBR && profile && maxKbps && mfx.CodecLevel) // maxKbps <= MaxBR[tierLevel]
            if (maxKbps > GetMaxBrForLevel(profile, mfx.CodecLevel) / 1000)
                wrnIncompatible = !CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBitrateInKbps(profile, maxKbps));

        if (IsCbrOrVbr(mfx.RateControlMethod) && fi.FrameRateExtN && fi.FrameRateExtD) {
            const uint32_t avgFrameInKB = (uint32_t)MIN(MAX_UINT/2, (double)targetKbps * fi.FrameRateExtD / fi.FrameRateExtN / 8);
            if (bufferSizeInKB) // bufferSizeInKB >= 2 average compressed frames
                wrnIncompatible = !CheckMinSat(bufferSizeInKB, avgFrameInKB * 2);
            if (initialDelayInKB) // initialDelayInKB >= 1 average compressed frames
                wrnIncompatible = !CheckMinSat(initialDelayInKB, avgFrameInKB);
        }
        if (opt2 && opt2->MaxFrameSize && mfx.RateControlMethod != CQP
            && fi.FrameRateExtN && fi.FrameRateExtD && fi.FourCC != P010 && fi.FourCC != P210
            && (fi.PicStruct == PROGR || (fi.PicStruct != PROGR && mfx.GopRefDist == 1))) {
            uint32_t MaxFrameSize = opt2->MaxFrameSize;
            const uint32_t avgFrameInBytes = (uint32_t)MIN(MAX_UINT / 2, (double)targetKbps * 1000 * fi.FrameRateExtD / fi.FrameRateExtN / 8) + 1;
            wrnIncompatible = !CheckMinSat(MaxFrameSize, avgFrameInBytes);
            opt2->MaxFrameSize = MaxFrameSize;
        }
        else if (opt2 && opt2->MaxFrameSize) {
            opt2->MaxFrameSize = 0, wrnIncompatible = true;
        }
        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && bufferSizeInKB) // bufferSizeInKB <= MaxCPB[High6.2]
            wrnIncompatible = !CheckMaxSat(bufferSizeInKB, GetMaxCpbForLevel(profile, H62) / 8000);

        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && initialDelayInKB) // initialDelayInKB <= MaxCPB[High6.2]
            wrnIncompatible = !CheckMaxSat(initialDelayInKB, GetMaxCpbForLevel(profile, H62) / 8000);

        if (IsCbrOrVbr(mfx.RateControlMethod) && bufferSizeInKB && initialDelayInKB) // initialDelayInKB <= bufferSizeInKB
            wrnIncompatible = !CheckMaxSat(initialDelayInKB, bufferSizeInKB);

        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && bufferSizeInKB && mfx.CodecLevel) // bufferSizeInKB <= MaxCPB[tierLevel]
            if (bufferSizeInKB > GetMaxCpbForLevel(profile, mfx.CodecLevel) / 8000)
                wrnIncompatible = !CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBufferSizeInKB(profile, bufferSizeInKB));

        if (IsCbrOrVbr(mfx.RateControlMethod) && profile && initialDelayInKB && mfx.CodecLevel) // initialDelayInKB <= MaxCPB[tierLevel]
            if (initialDelayInKB > GetMaxCpbForLevel(profile, mfx.CodecLevel) / 8000)
                wrnIncompatible = !CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBufferSizeInKB(profile, initialDelayInKB));

        // if any of brc parameters changed, find new BRCParamMultiplier (min possible)
        bool recalibrateBrcParamMultiplier =
            (targetKbps       != (uint32_t)(MAX(1, mfx.BRCParamMultiplier) * mfx.TargetKbps))     ||
            (maxKbps          != (uint32_t)(MAX(1, mfx.BRCParamMultiplier) * mfx.MaxKbps))        ||
            (bufferSizeInKB   != (uint32_t)(MAX(1, mfx.BRCParamMultiplier) * mfx.BufferSizeInKB)) ||
            (initialDelayInKB != (uint32_t)(MAX(1, mfx.BRCParamMultiplier) * mfx.InitialDelayInKB));
        if (recalibrateBrcParamMultiplier) {
            assert(mfx.RateControlMethod != CQP);
            uint32_t maxBrcValue = targetKbps;
            if (mfx.RateControlMethod == VBR)
                maxBrcValue = MAX(maxBrcValue, maxKbps);
            if (IsCbrOrVbr(mfx.RateControlMethod))
                maxBrcValue = MAX(maxBrcValue, MAX(initialDelayInKB, bufferSizeInKB));
            mfx.BRCParamMultiplier = uint16_t((maxBrcValue + 0x10000) / 0x10000);
            if (mfx.TargetKbps)
                mfx.TargetKbps = uint16_t(targetKbps / mfx.BRCParamMultiplier);
            if (mfx.RateControlMethod == VBR && mfx.MaxKbps)
                mfx.MaxKbps = uint16_t(maxKbps / mfx.BRCParamMultiplier);
            if (IsCbrOrVbr(mfx.RateControlMethod)) {
                if (mfx.BufferSizeInKB)
                    mfx.BufferSizeInKB = uint16_t(bufferSizeInKB / mfx.BRCParamMultiplier);
                if (mfx.InitialDelayInKB)
                    mfx.InitialDelayInKB = uint16_t(initialDelayInKB / mfx.BRCParamMultiplier);
            }
        }

        if (optHevc && optHevc->FramesInParallel == 1 && (fi.PicStruct == TFF || fi.PicStruct == BFF)) // framesInParallel must be at least 2 for Interlace
            optHevc->FramesInParallel = 2, wrnIncompatible = true;

        //if (mfx.NumSlice > 1 && optHevc && optHevc->FramesInParallel > 1) // either multi-slice or frame-threading
        //    optHevc->FramesInParallel = 1, wrnIncompatible = true;

        //if (tiles && (tiles->NumTileRows > 1 || tiles->NumTileColumns > 1) && optHevc && optHevc->FramesInParallel > 1) // either multi-tile or frame-threading
        //    optHevc->FramesInParallel = 1, wrnIncompatible = true;

        if (optHevc && optHevc->EnableCm == ON && optHevc->FramesInParallel > 8) // no more than 8 parallel frames in gacc
            optHevc->FramesInParallel = 8, wrnIncompatible = true;

        //if (mfx.NumSlice && region && region->RegionEncoding == MFX_HEVC_REGION_ENCODING_ON && region->RegionType == MFX_HEVC_REGION_SLICE && region->RegionId >= mfx.NumSlice) // RegionId < NumSlice
        //    region->RegionId = 0, errInvalidParam = true;

        if (optHevc && optHevc->Log2MaxCUSize && optHevc->MaxCUDepth > optHevc->Log2MaxCUSize - 2) // MaxCUDepth <= Log2MaxCUSize - 2
            optHevc->MaxCUDepth = optHevc->Log2MaxCUSize - 2, wrnIncompatible = true;

        if (picWidth && optHevc && optHevc->Log2MaxCUSize && optHevc->MaxCUDepth) // Width % MinCuSize == 0
            while (picWidth & ((1 << (optHevc->Log2MaxCUSize - optHevc->MaxCUDepth + 1)) - 1))
                optHevc->MaxCUDepth++, wrnIncompatible = true;

        if (picHeight && optHevc && optHevc->Log2MaxCUSize && optHevc->MaxCUDepth) // Height % MinCuSize == 0
            while (picHeight & ((1 << (optHevc->Log2MaxCUSize - optHevc->MaxCUDepth + 1)) - 1))
                optHevc->MaxCUDepth++, wrnIncompatible = true;

        if (optHevc && optHevc->Log2MaxCUSize && optHevc->QuadtreeTULog2MaxSize > optHevc->Log2MaxCUSize) // MaxTU <= CTB
            optHevc->QuadtreeTULog2MaxSize = optHevc->Log2MaxCUSize, wrnIncompatible = true;

        if (optHevc && optHevc->Log2MaxCUSize && optHevc->QuadtreeTULog2MinSize > optHevc->Log2MaxCUSize) // MinTU <= CTB
            optHevc->QuadtreeTULog2MinSize = optHevc->Log2MaxCUSize, wrnIncompatible = true;

        if (optHevc && optHevc->Log2MaxCUSize && optHevc->MaxCUDepth && optHevc->QuadtreeTULog2MinSize) // MinTU <= MinCU
            wrnIncompatible = !CheckMaxSat(optHevc->QuadtreeTULog2MinSize, optHevc->Log2MaxCUSize - optHevc->MaxCUDepth + 1);

        if (optHevc && optHevc->QuadtreeTULog2MinSize && optHevc->QuadtreeTULog2MaxSize) // MaxTU >= MinTU
            wrnIncompatible = !CheckMinSat(optHevc->QuadtreeTULog2MaxSize, optHevc->QuadtreeTULog2MinSize);

        if (optHevc && optHevc->ForceNumThread && mfx.NumThread) // ForceNumThread != NumThread
            wrnIncompatible = !CheckEq(mfx.NumThread, optHevc->ForceNumThread);

        if (optHevc && optHevc->DeltaQpMode) {
            if (fi.FourCC == P010 || fi.FourCC == P210) { // only CAQ alllowed for 10 bit
                if ((optHevc->DeltaQpMode - 1) & (AMT_DQP_CAL | AMT_DQP_PAQ))
                    optHevc->DeltaQpMode = 1 + ((optHevc->DeltaQpMode - 1) & AMT_DQP_CAQ), wrnIncompatible = true;
            }
            if (optHevc->BPyramid == OFF || mfx.GopRefDist == 1) { // no CAL/PAQ if Bpyramid disabled
                if ((optHevc->DeltaQpMode - 1) & (AMT_DQP_CAL | AMT_DQP_PAQ))
                    optHevc->DeltaQpMode = 1 + ((optHevc->DeltaQpMode - 1) & AMT_DQP_CAQ), wrnIncompatible = true;
            }
        }

        // conflict MBBRC vs GopRefDist
        if (opt2 && opt2->MBBRC == ON && (optHevc && optHevc->BPyramid == OFF || mfx.GopRefDist == 1 || optHevc && optHevc->DeltaQpMode == 1))
            opt2->MBBRC = OFF, wrnIncompatible = true;

        if ((mfx.GopOptFlag & MFX_GOP_STRICT) && opt2 && opt2->AdaptiveI)
            wrnIncompatible = !CheckEq(opt2->AdaptiveI, OFF);

        if (tlayers) {
            if (tlayers->Layer[0].Scale && tlayers->Layer[0].Scale != 1) {
                tlayers->Layer[0].Scale = 1;
                wrnIncompatible = true;
            }

            for (int32_t i = 3; i < 8; i++) {
                if (tlayers->Layer[i].Scale) {
                    tlayers->Layer[i].Scale = 0;
                    wrnIncompatible = true;
                }
            }
            if ((tlayers->Layer[1].Scale || tlayers->Layer[2].Scale) && (mfx.GopRefDist!=1 || mfx.EncodedOrder || (opt3 && opt3->PRefType==1))) {
                tlayers->Layer[1].Scale = 0;
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
            if (tlayers->Layer[1].Scale && tlayers->Layer[1].Scale!=2) {
                tlayers->Layer[1].Scale = 0;
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
            if (tlayers->Layer[2].Scale && tlayers->Layer[2].Scale != 4) {
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
        }

        if (dumpFiles) {
            size_t MAXLEN = sizeof(dumpFiles->ReconFilename) / sizeof(dumpFiles->ReconFilename[0]);
            vm_char *pzero = std::find(dumpFiles->ReconFilename, dumpFiles->ReconFilename + MAXLEN, 0);
            if (pzero == dumpFiles->ReconFilename + MAXLEN)
                dumpFiles->ReconFilename[0] = 0, wrnIncompatible = true; // too long name

            MAXLEN = sizeof(dumpFiles->InputFramesFilename) / sizeof(dumpFiles->InputFramesFilename[0]);
            pzero = std::find(dumpFiles->InputFramesFilename, dumpFiles->InputFramesFilename + MAXLEN, 0);
            if (pzero == dumpFiles->InputFramesFilename + MAXLEN)
                dumpFiles->InputFramesFilename[0] = 0, wrnIncompatible = true; // too long name
        }

        if      (errInvalidParam) return MFX_ERR_INVALID_VIDEO_PARAM;
        else if (errUnsupported)  return MFX_ERR_UNSUPPORTED;
        else if (wrnIncompatible) return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        else                      return MFX_ERR_NONE;
    }


    void SetDefaultValues(mfxVideoParam &par)
    {
        mfxInfoMFX &mfx = par.mfx;
        mfxFrameInfo &fi = par.mfx.FrameInfo;
        mfxExtHEVCTiles &tiles = GetExtBuffer(par);
        mfxExtCodingOptionAV1E &optHevc = GetExtBuffer(par);
        mfxExtHEVCParam &hevcParam = GetExtBuffer(par);
        mfxExtCodingOption &opt = GetExtBuffer(par);
        mfxExtCodingOption2 &opt2 = GetExtBuffer(par);
        mfxExtCodingOption3 &opt3 = GetExtBuffer(par);
        mfxExtHEVCRegion &region = GetExtBuffer(par);
        mfxExtEncoderROI &roi = GetExtBuffer(par);
        mfxExtAvcTemporalLayers &tlayers = GetExtBuffer(par);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        mfxExtAV1Param &av1param = GetExtBuffer(par);
#endif

        if (hevcParam.PicWidthInLumaSamples == 0)
            hevcParam.PicWidthInLumaSamples = fi.Width;
        if (hevcParam.PicHeightInLumaSamples == 0)
            hevcParam.PicHeightInLumaSamples = fi.Height;

        const uint32_t lumaPs = hevcParam.PicWidthInLumaSamples * hevcParam.PicHeightInLumaSamples;

        if (par.AsyncDepth == 0)
            par.AsyncDepth = 3;
        if (mfx.BRCParamMultiplier == 0)
            mfx.BRCParamMultiplier = 1;
        if (mfx.CodecProfile == 0)
            mfx.CodecProfile = uint16_t(GetProfile(fi.FourCC));
#if USE_SRC10ENC8PAK10
        if (fi.FourCC == MFX_FOURCC_P010) {
            mfx.CodecProfile = uint16_t(GetProfile(MFX_FOURCC_NV12));
        }
#endif
        if (hevcParam.GeneralConstraintFlags == 0 && mfx.CodecProfile == REXT)
            hevcParam.GeneralConstraintFlags = MAIN_422_10;
        if (mfx.TargetUsage == 0)
            mfx.TargetUsage = 4;
        if (mfx.RateControlMethod == 0)
            mfx.RateControlMethod = AVBR;
        if (mfx.NumSlice == 0)
            mfx.NumSlice = 1;
        if (fi.CropW == 0)
            fi.CropW = hevcParam.PicWidthInLumaSamples;
        if (fi.CropH == 0)
            fi.CropH = hevcParam.PicHeightInLumaSamples;
        if (fi.PicStruct == 0)
            fi.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        if (optHevc.ForceNumThread == 0)
            optHevc.ForceNumThread = mfx.NumThread ? mfx.NumThread : (uint16_t)vm_sys_info_get_cpu_num();
        if (mfx.NumThread == 0)
            mfx.NumThread = optHevc.ForceNumThread;
        if (optHevc.EnableCm == 0)
            optHevc.EnableCm = DEFAULT_ENABLE_CM;
        if (mfx.GopRefDist == 0)
            mfx.GopRefDist = mfx.GopPicSize ? MIN(mfx.GopPicSize, 8) : 8;

        const mfxExtCodingOptionAV1E &defaultOptHevc = (optHevc.EnableCm == OFF ? tab_defaultOptHevcSw : tab_defaultOptHevcGacc)[mfx.TargetUsage];

        if (optHevc.ScreenMode == 0)
            optHevc.ScreenMode = defaultOptHevc.ScreenMode;

        //if (tiles.NumTileRows == 0)
        //    tiles.NumTileRows = 1;
        //if (tiles.NumTileColumns == 0) {
        //    tiles.NumTileColumns = 1 << BSR(std::max((int32_t)1, fi.Width / CodecLimits::MIN_TILE_WIDTH));
        //    tiles.NumTileColumns = std::min((uint16_t)2, tiles.NumTileColumns);    // dont need many tiles with wpp execution of mode decision
        //}
        //const int32_t numTile = tiles.NumTileRows * tiles.NumTileColumns;
        if (optHevc.NumTileColumnsKeyFrame == 0) {
            //optHevc.NumTileColumnsKeyFrame = 1 << BSR(std::max((int32_t)1, fi.Width / CodecLimits::MIN_TILE_WIDTH));
            //optHevc.NumTileColumnsKeyFrame = std::min((uint16_t)2, optHevc.NumTileColumnsKeyFrame);    // dont need many tiles with wpp execution of mode decision
            optHevc.NumTileColumnsKeyFrame = (par.AsyncDepth == 1 && fi.CropW >= CodecLimits::MIN_TILE_WIDTH_IMPL) ? 2 : 1;
        }
        if (optHevc.NumTileColumnsInterFrame == 0) {
            int32_t size = defaultOptHevc.Log2MaxCUSize;
            float wppEff = MIN((mfx.FrameInfo.Height + (1 << size) - 1) >> size,
                                (mfx.FrameInfo.Width + (1 << size) - 1) >> (size + 1)) / 2.75f;
            optHevc.NumTileColumnsInterFrame = (par.AsyncDepth == 1 && mfx.GopRefDist == 1 && wppEff < mfx.NumThread  && fi.CropW >= CodecLimits::MIN_TILE_WIDTH_IMPL) ? 2 : 1;
        }


        if (optHevc.NumRefLayers == 0) {
            if (mfx.GopRefDist == 1) {
                optHevc.NumRefLayers = 1;
            } else {
                optHevc.NumRefLayers = defaultOptHevc.NumRefLayers;
            }
        }

        if (tlayers.Layer[0].Scale == 0) {
            tlayers.Layer[0].Scale = 1;
            tlayers.Layer[1].Scale = 0;
            if (mfx.GopRefDist == 1 && mfx.NumRefFrame != 1 && opt3.PRefType != 1) {
                if (optHevc.NumRefLayers == 2)
                    tlayers.Layer[1].Scale = 2;
                else if (optHevc.NumRefLayers >= 3) {
                    tlayers.Layer[1].Scale = 2;
                    tlayers.Layer[2].Scale = 4;
                }
            }
        }

        if (opt3.ExtBrcAdaptiveLTR == 0 && mfx.GopRefDist == 1)
            opt3.ExtBrcAdaptiveLTR = ON;

        if (optHevc.BPyramid == 0)
            optHevc.BPyramid = defaultOptHevc.BPyramid;

        uint8_t defaultNumRefFrame = ((mfx.GopRefDist == 1)
            ? (optHevc.EnableCm == OFF ? tab_defaultNumRefFrameLowDelaySw : tab_defaultNumRefFrameLowDelayGacc)
            : (optHevc.EnableCm == OFF ? tab_defaultNumRefFrameSw : tab_defaultNumRefFrameGacc)) [mfx.TargetUsage];

        if (optHevc.FramesInParallel == 0) {
            if (par.AsyncDepth == 1)
                optHevc.FramesInParallel = (fi.PicStruct == TFF || fi.PicStruct == BFF) ? 2 : 1; // at least 2 for Interlace
            else if (optHevc.EnableCm == ON) {
                if (mfx.GopPicSize == 1) {
                    // All Intra
                    optHevc.FramesInParallel = MIN(par.AsyncDepth, 4);
                } else {
                    const mfxU16 MIN_FRAMES_IN_PARALLEL = 2;
                    const mfxU16 MAX_FRAMES_IN_PARALLEL = 7;
                    optHevc.FramesInParallel = std::max(MIN_FRAMES_IN_PARALLEL, std::min(MAX_FRAMES_IN_PARALLEL, mfx.GopRefDist));
                }
            } else {
                if      (mfx.NumThread >= 48) optHevc.FramesInParallel = 8;
                else if (mfx.NumThread >= 32) optHevc.FramesInParallel = 7;
                else if (mfx.NumThread >= 16) optHevc.FramesInParallel = 5;
                else if (mfx.NumThread >=  8) optHevc.FramesInParallel = 3;
                else if (mfx.NumThread >=  4) optHevc.FramesInParallel = 2;
                else                          optHevc.FramesInParallel = 1;

                int32_t size = defaultOptHevc.Log2MaxCUSize;
                float wppEff = MIN((mfx.FrameInfo.Height + (1 << size) - 1) >> size,
                                    (mfx.FrameInfo.Width  + (1 << size) - 1) >> (size + 1) ) / 2.75f;
                //float wppEff = MIN((mfx.FrameInfo.Height + (1 << size) - 1) >> size,
                //                    (mfx.FrameInfo.Width  + (1 << size) - 1) >> size ) / 2.75f;
                float frameMult = MAX(1.0f, MIN((float)mfx.NumThread / (float)wppEff, 4.0f));
                if (mfx.NumThread >= 4)
                    optHevc.FramesInParallel = MIN(uint16_t(optHevc.FramesInParallel * frameMult + 0.5f), 16);
            }
        }

        //if (optHevc.WPP == 0)
        //    optHevc.WPP = uint16_t((numTile > 1 || mfx.NumThread == 1) ? OFF : ON);

        if (mfx.NumRefFrame == 0) {
            uint16_t level = mfx.CodecLevel ? mfx.CodecLevel : MFX_LEVEL_HEVC_62;
            mfx.NumRefFrame = MIN(GetMaxDpbSize(lumaPs, level), defaultNumRefFrame);
        }

        if (IsCbrOrVbr(mfx.RateControlMethod)) {
            if (mfx.BufferSizeInKB == 0) {
                uint16_t level = mfx.CodecLevel ? mfx.CodecLevel : MFX_LEVEL_HEVC_62;
                int32_t maxCpbInKB = MIN(0xffff, GetMaxCpbForLevel(mfx.CodecProfile, level) / 8000 / mfx.BRCParamMultiplier);
                mfx.BufferSizeInKB = (uint16_t)Saturate(1, maxCpbInKB, mfx.TargetKbps / 4); // 2 second buffer
                if (mfx.BufferSizeInKB < mfx.InitialDelayInKB)
                    mfx.BufferSizeInKB = mfx.InitialDelayInKB;
            }

            if (mfx.InitialDelayInKB == 0) {
                if (opt2.MaxFrameSize)
                    mfx.InitialDelayInKB = opt2.MaxFrameSize / 1000 / mfx.BRCParamMultiplier;
                else
                    mfx.InitialDelayInKB = MAX(1, mfx.BufferSizeInKB * 3 / 4); // 75% fullness
            }

            if (mfx.RateControlMethod == VBR && mfx.MaxKbps == 0) {
                uint16_t level = mfx.CodecLevel ? mfx.CodecLevel : MFX_LEVEL_HEVC_62;
                int32_t maxBrInKB = MIN(0xffff, GetMaxBrForLevel(mfx.CodecProfile, level) / 1000 / mfx.BRCParamMultiplier);
                mfx.MaxKbps = (uint16_t)MIN(maxBrInKB, mfx.TargetKbps * 3 / 2); // 150% of target rate
            }
        } else {
            if (mfx.BufferSizeInKB == 0) {
                // for dynamic resolution up to 1080p
                int maxDynLumaPs = MAX(lumaPs, 1920*1080);
                const int numFrames = mfx.GopRefDist == 1 ? 1 : 4;
                mfx.BufferSizeInKB = (uint16_t)Saturate(1, 0xffff, numFrames * maxDynLumaPs * GetBpp(fi.FourCC) / 8000 / mfx.BRCParamMultiplier);
            }
            if (mfx.RateControlMethod == CQP) {
                if (mfx.GopRefDist == 1) {
                    if (mfx.QPI == 0)
                        mfx.QPI = (uint16_t)Saturate(MINQ, MAXQ, mfx.QPP ? (mfx.QPP - 20) : mfx.QPB ? (mfx.QPB - 20) : 128);
                    if (mfx.QPP == 0)
#ifdef USE_OLD_IPB_QINDEX
                        mfx.QPP = MIN(MAXQ, mfx.QPI + 20);
#else
                        mfx.QPP = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[mfx.QPI])]);
#endif
                    if (mfx.QPB == 0)
                        mfx.QPB = MIN(MAXQ, mfx.QPI + 20);
                } else {
                    if (mfx.QPI == 0)
                        mfx.QPI = Saturate(MINQ, MAXQ, mfx.QPP ? (mfx.QPP - 10) : mfx.QPB ? (mfx.QPB - 30) : 128);
                    if (mfx.QPP == 0) {
#ifdef USE_OLD_IPB_QINDEX
                        mfx.QPP = MIN(MAXQ, mfx.QPI + 10);
#else
                        mfx.QPP = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[mfx.QPI])]);
#endif
                    }
                    if (mfx.QPB == 0) {
#ifdef USE_OLD_IPB_QINDEX
                        mfx.QPB = MIN(MAXQ, mfx.QPP + 20);
#else
                        mfx.QPB = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[mfx.QPP])])]);
#endif
                    }
                }
            }
        }

        // set minimal Level
        if (mfx.CodecLevel == 0) {
            mfx.CodecLevel = GetMinLevelForLumaPs(lumaPs);
            CheckMinLevel(mfx.CodecLevel, GetMinLevelForLumaSr(lumaPs, fi.FrameRateExtN, fi.FrameRateExtD));
            CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumSlice(mfx.NumSlice));
            CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumTileRows(tiles.NumTileRows));
            CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumTileCols(tiles.NumTileColumns));
            CheckMinLevel(mfx.CodecLevel, GetMinLevelForNumRefFrame(lumaPs, mfx.NumRefFrame));

            if (IsCbrOrVbr(mfx.RateControlMethod)) {
                uint32_t cpb = mfx.BRCParamMultiplier * mfx.BufferSizeInKB;
                uint32_t br  = mfx.BRCParamMultiplier * (mfx.RateControlMethod == CBR ? mfx.TargetKbps : mfx.MaxKbps);
                CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBufferSizeInKB(mfx.CodecProfile, cpb));
                CheckMinTierLevel(mfx.CodecLevel, GetMinTierLevelForBitrateInKbps(mfx.CodecProfile, br));
            }
        }

        if (opt.AUDelimiter == 0)
            opt.AUDelimiter = ON;

        if (opt2.DisableVUI == 0)
            opt2.DisableVUI = OFF;

        if (opt2.AdaptiveI == 0) {
            if (mfx.GopOptFlag & MFX_GOP_STRICT)
                opt2.AdaptiveI = OFF;
            else if (IsCbrOrVbrOrAvbr(mfx.RateControlMethod))
                opt2.AdaptiveI = ON;
            else
                opt2.AdaptiveI = OFF;
        }

        if (opt3.GPB == 0) {
            opt3.GPB = ON;
        }

        if (optHevc.Log2MaxCUSize == 0) {
            optHevc.Log2MaxCUSize = defaultOptHevc.Log2MaxCUSize;
            //if (optHevc.Log2MaxCUSize == 6 && ((hevcParam.PicHeightInLumaSamples + 63) >> 6) < mfx.NumSlice)
            //    optHevc.Log2MaxCUSize = 5;
        }
        if (optHevc.MaxCUDepth == 0)
            optHevc.MaxCUDepth = defaultOptHevc.MaxCUDepth;
        if (optHevc.QuadtreeTULog2MaxSize == 0)
            optHevc.QuadtreeTULog2MaxSize = defaultOptHevc.QuadtreeTULog2MaxSize;
        if (optHevc.QuadtreeTULog2MinSize == 0)
            optHevc.QuadtreeTULog2MinSize = defaultOptHevc.QuadtreeTULog2MinSize;
        if (optHevc.QuadtreeTUMaxDepthIntra == 0)
            optHevc.QuadtreeTUMaxDepthIntra = defaultOptHevc.QuadtreeTUMaxDepthIntra;
        if (optHevc.QuadtreeTUMaxDepthInter == 0)
            optHevc.QuadtreeTUMaxDepthInter = defaultOptHevc.QuadtreeTUMaxDepthInter;
        if (optHevc.QuadtreeTUMaxDepthInterRD == 0)
            optHevc.QuadtreeTUMaxDepthInterRD = defaultOptHevc.QuadtreeTUMaxDepthInterRD;
        if (optHevc.IntraNumCand0_2 == 0)
            optHevc.IntraNumCand0_2 = defaultOptHevc.IntraNumCand0_2;
        if (optHevc.IntraNumCand0_3 == 0)
            optHevc.IntraNumCand0_3 = defaultOptHevc.IntraNumCand0_3;
        if (optHevc.IntraNumCand0_4 == 0)
            optHevc.IntraNumCand0_4 = defaultOptHevc.IntraNumCand0_4;
        if (optHevc.IntraNumCand0_5 == 0)
            optHevc.IntraNumCand0_5 = defaultOptHevc.IntraNumCand0_5;
        if (optHevc.IntraNumCand0_6 == 0)
            optHevc.IntraNumCand0_6 = defaultOptHevc.IntraNumCand0_6;
        if (optHevc.IntraNumCand1_2 == 0)
            optHevc.IntraNumCand1_2 = defaultOptHevc.IntraNumCand1_2;
        if (optHevc.IntraNumCand1_3 == 0)
            optHevc.IntraNumCand1_3 = defaultOptHevc.IntraNumCand1_3;
        if (optHevc.IntraNumCand1_4 == 0)
            optHevc.IntraNumCand1_4 = defaultOptHevc.IntraNumCand1_4;
        if (optHevc.IntraNumCand1_5 == 0)
            optHevc.IntraNumCand1_5 = defaultOptHevc.IntraNumCand1_5;
        if (optHevc.IntraNumCand1_6 == 0)
            optHevc.IntraNumCand1_6 = defaultOptHevc.IntraNumCand1_6;
        if (optHevc.IntraNumCand2_2 == 0)
            optHevc.IntraNumCand2_2 = defaultOptHevc.IntraNumCand2_2;
        if (optHevc.IntraNumCand2_3 == 0)
            optHevc.IntraNumCand2_3 = defaultOptHevc.IntraNumCand2_3;
        if (optHevc.IntraNumCand2_4 == 0)
            optHevc.IntraNumCand2_4 = defaultOptHevc.IntraNumCand2_4;
        if (optHevc.IntraNumCand2_5 == 0)
            optHevc.IntraNumCand2_5 = defaultOptHevc.IntraNumCand2_5;
        if (optHevc.IntraNumCand2_6 == 0)
            optHevc.IntraNumCand2_6 = defaultOptHevc.IntraNumCand2_6;
        if (optHevc.AnalyzeChroma == 0)
            optHevc.AnalyzeChroma = defaultOptHevc.AnalyzeChroma;
        if (optHevc.SignBitHiding == 0)
            optHevc.SignBitHiding = defaultOptHevc.SignBitHiding;
        if (optHevc.RDOQuant == 0)
            optHevc.RDOQuant = defaultOptHevc.RDOQuant;
        if (optHevc.SAO == 0)
            optHevc.SAO = defaultOptHevc.SAO;
        if (optHevc.SplitThresholdStrengthCUIntra == 0)
            optHevc.SplitThresholdStrengthCUIntra = defaultOptHevc.SplitThresholdStrengthCUIntra;
        if (optHevc.SplitThresholdStrengthTUIntra == 0)
            optHevc.SplitThresholdStrengthTUIntra = defaultOptHevc.SplitThresholdStrengthTUIntra;
        if (optHevc.SplitThresholdStrengthCUInter == 0)
            optHevc.SplitThresholdStrengthCUInter = defaultOptHevc.SplitThresholdStrengthCUInter;
        if (optHevc.SplitThresholdMultiplier == 0)
            optHevc.SplitThresholdMultiplier = defaultOptHevc.SplitThresholdMultiplier;
        if (optHevc.PartModes == 0)
            optHevc.PartModes = defaultOptHevc.PartModes;
        if (optHevc.CmIntraThreshold == 0)
            optHevc.CmIntraThreshold = defaultOptHevc.CmIntraThreshold;
        if (optHevc.TUSplitIntra == 0)
            optHevc.TUSplitIntra = defaultOptHevc.TUSplitIntra;
        if (optHevc.CUSplit == 0)
            optHevc.CUSplit = defaultOptHevc.CUSplit;
        if (optHevc.IntraAngModes == 0)
            optHevc.IntraAngModes = defaultOptHevc.IntraAngModes;
        if (optHevc.EnableCm == 0)
            optHevc.EnableCm = defaultOptHevc.EnableCm;
        if (optHevc.EnableCmInterp == 0)
            optHevc.EnableCmInterp = defaultOptHevc.EnableCmInterp;
        if (optHevc.FastPUDecision == 0)
            optHevc.FastPUDecision = defaultOptHevc.FastPUDecision;
        if (optHevc.HadamardMe == 0)
            optHevc.HadamardMe = defaultOptHevc.HadamardMe;
        if (optHevc.TMVP == 0)
            optHevc.TMVP = defaultOptHevc.TMVP;
        if (optHevc.Deblocking == 0)
            optHevc.Deblocking = defaultOptHevc.Deblocking;
        if (optHevc.RDOQuantChroma == 0)
            optHevc.RDOQuantChroma = defaultOptHevc.RDOQuantChroma;
        if (optHevc.RDOQuantCGZ == 0)
            optHevc.RDOQuantCGZ = defaultOptHevc.RDOQuantCGZ;
        if (optHevc.SaoOpt == 0)
            optHevc.SaoOpt = defaultOptHevc.SaoOpt;
        if (optHevc.SaoSubOpt == 0)
            optHevc.SaoSubOpt = defaultOptHevc.SaoSubOpt;
        if (optHevc.CostChroma == 0)
            optHevc.CostChroma = defaultOptHevc.CostChroma;
        if (optHevc.PatternIntPel == 0)
            optHevc.PatternIntPel = defaultOptHevc.PatternIntPel;
        if (optHevc.FastSkip == 0)
            optHevc.FastSkip = defaultOptHevc.FastSkip;
        if (optHevc.PatternSubPel == 0)
            optHevc.PatternSubPel = defaultOptHevc.PatternSubPel;
        if (optHevc.FastCbfMode == 0)
            optHevc.FastCbfMode = defaultOptHevc.FastCbfMode;
        if (optHevc.PuDecisionSatd == 0)
            optHevc.PuDecisionSatd = defaultOptHevc.PuDecisionSatd;
        if (optHevc.MinCUDepthAdapt == 0)
            optHevc.MinCUDepthAdapt = defaultOptHevc.MinCUDepthAdapt;
        if (optHevc.MaxCUDepthAdapt == 0)
            optHevc.MaxCUDepthAdapt = defaultOptHevc.MaxCUDepthAdapt;
        if (optHevc.NumBiRefineIter == 0)
            optHevc.NumBiRefineIter = defaultOptHevc.NumBiRefineIter;
        if (optHevc.CUSplitThreshold == 0)
            optHevc.CUSplitThreshold = defaultOptHevc.CUSplitThreshold;
        if (optHevc.FwdProbUpdateCoef == 0)
            optHevc.FwdProbUpdateCoef = defaultOptHevc.FwdProbUpdateCoef;
        if (optHevc.FwdProbUpdateSyntax == 0)
            optHevc.FwdProbUpdateSyntax = defaultOptHevc.FwdProbUpdateSyntax;

        if (optHevc.DeltaQpMode == 0) {
            optHevc.DeltaQpMode = defaultOptHevc.DeltaQpMode;
            if (optHevc.DeltaQpMode > 2 && (fi.FourCC == P010 || fi.FourCC == P210))
                optHevc.DeltaQpMode = 2; // CAQ only
            if (optHevc.DeltaQpMode > 2 && (mfx.GopRefDist == 1 || optHevc.BPyramid == OFF))
                optHevc.DeltaQpMode = 1 + ((optHevc.DeltaQpMode - 1) & (AMT_DQP_CAQ | AMT_DQP_PSY | AMT_DQP_HROI)); // CAQ, PSY, HROI only
            if (roi.NumROI)
                optHevc.DeltaQpMode = 1; // off
        }

        if (opt2.MBBRC == 0) {
            if ((optHevc.BPyramid == OFF || mfx.GopRefDist == 1 || optHevc.DeltaQpMode == 1))
                opt2.MBBRC = OFF;
            else
                opt2.MBBRC = ON;
        }

        if (optHevc.Enable10bit == 0)
            optHevc.Enable10bit = defaultOptHevc.Enable10bit;
        if (optHevc.IntraAngModesP == 0)
            optHevc.IntraAngModesP = defaultOptHevc.IntraAngModesP;
        if (optHevc.IntraAngModesBRef == 0)
            optHevc.IntraAngModesBRef = defaultOptHevc.IntraAngModesBRef;
        if (optHevc.IntraAngModesBnonRef == 0)
            optHevc.IntraAngModesBnonRef = defaultOptHevc.IntraAngModesBnonRef;
        if (optHevc.IntraChromaRDO == 0)
            optHevc.IntraChromaRDO = defaultOptHevc.IntraChromaRDO;
        if (optHevc.FastInterp == 0)
            optHevc.FastInterp = defaultOptHevc.FastInterp;
        if (optHevc.SplitThresholdTabIndex == 0)
            optHevc.SplitThresholdTabIndex = defaultOptHevc.SplitThresholdTabIndex;
        if (optHevc.CpuFeature == 0)
            optHevc.CpuFeature = defaultOptHevc.CpuFeature;
        if (optHevc.TryIntra == 0)
            optHevc.TryIntra = defaultOptHevc.TryIntra;
        if (optHevc.FastAMPSkipME == 0)
            optHevc.FastAMPSkipME = defaultOptHevc.FastAMPSkipME;
        if (optHevc.FastAMPRD == 0)
            optHevc.FastAMPRD = defaultOptHevc.FastAMPRD;
        if (optHevc.SkipMotionPartition == 0)
            optHevc.SkipMotionPartition = defaultOptHevc.SkipMotionPartition;
        if (optHevc.SkipCandRD == 0)
            optHevc.SkipCandRD = defaultOptHevc.SkipCandRD;
        if (optHevc.AdaptiveRefs == 0)
            optHevc.AdaptiveRefs = defaultOptHevc.AdaptiveRefs;
        if (optHevc.FastCoeffCost == 0)
            optHevc.FastCoeffCost = defaultOptHevc.FastCoeffCost;
        if (optHevc.NumRefFrameB == 0)
            optHevc.NumRefFrameB = defaultOptHevc.NumRefFrameB;
        if (optHevc.IntraMinDepthSC == 0)
            optHevc.IntraMinDepthSC = defaultOptHevc.IntraMinDepthSC;
        if (optHevc.InterMinDepthSTC == 0)
            optHevc.InterMinDepthSTC = defaultOptHevc.InterMinDepthSTC;
        if (optHevc.MotionPartitionDepth == 0)
            optHevc.MotionPartitionDepth = defaultOptHevc.MotionPartitionDepth;
        if (optHevc.AnalyzeCmplx == 0)
            optHevc.AnalyzeCmplx = IsCbrOrVbrOrAvbr(mfx.RateControlMethod) ? 2 : 1;
        if (optHevc.RateControlDepth == 0)
            if (optHevc.AnalyzeCmplx == 2)
                if ((mfx.GopPicSize == 1 || mfx.GopRefDist == 1) && par.AsyncDepth == 1)
                    optHevc.RateControlDepth = 0;
                else
                    optHevc.RateControlDepth = MAX(3,mfx.GopRefDist + 1);
        if (optHevc.LowresFactor == 0)
            optHevc.LowresFactor = defaultOptHevc.LowresFactor;
        if (optHevc.DeblockBorders == 0 && region.RegionEncoding == MFX_HEVC_REGION_ENCODING_OFF)
            optHevc.DeblockBorders = defaultOptHevc.DeblockBorders;
        if (optHevc.SAOChroma == 0)
            optHevc.SAOChroma = defaultOptHevc.SAOChroma;
        if (optHevc.RepackForMaxFrameSize == 0)
            optHevc.RepackForMaxFrameSize = defaultOptHevc.RepackForMaxFrameSize;
        if (optHevc.DeblockingLevelMethod == 0)
            optHevc.DeblockingLevelMethod = defaultOptHevc.DeblockingLevelMethod;
        if (optHevc.AllowHpMv == 0)
            optHevc.AllowHpMv = defaultOptHevc.AllowHpMv;
        if (optHevc.MaxTxDepthIntra == 0)
            optHevc.MaxTxDepthIntra = defaultOptHevc.MaxTxDepthIntra;
        if (optHevc.MaxTxDepthInter == 0)
            optHevc.MaxTxDepthInter = defaultOptHevc.MaxTxDepthInter;
        if (optHevc.MaxTxDepthIntraRefine == 0)
            optHevc.MaxTxDepthIntraRefine = defaultOptHevc.MaxTxDepthIntraRefine;
        if (optHevc.MaxTxDepthInterRefine == 0)
            optHevc.MaxTxDepthInterRefine = defaultOptHevc.MaxTxDepthInterRefine;
        if (optHevc.ChromaRDO == 0)
            optHevc.ChromaRDO = defaultOptHevc.ChromaRDO;
        if (optHevc.InterpFilter == 0)
            optHevc.InterpFilter = defaultOptHevc.InterpFilter;
        if (optHevc.InterpFilterRefine == 0)
            optHevc.InterpFilterRefine = defaultOptHevc.InterpFilterRefine;
        if (optHevc.IntraRDO == 0)
            optHevc.IntraRDO = defaultOptHevc.IntraRDO;
        if (optHevc.InterRDO == 0)
            optHevc.InterRDO = defaultOptHevc.InterRDO;
        if (optHevc.IntraInterRDO == 0)
            optHevc.IntraInterRDO = defaultOptHevc.IntraInterRDO;
        if (optHevc.CodecTypeExt == 0)
            optHevc.CodecTypeExt = defaultOptHevc.CodecTypeExt;
        if (optHevc.CDEF == 0)
            optHevc.CDEF = defaultOptHevc.CDEF;
        if (optHevc.LRMode == 0)
            optHevc.LRMode = defaultOptHevc.LRMode;
        if (optHevc.SRMode == 0)
            optHevc.SRMode = defaultOptHevc.SRMode;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        if (av1param.EnableCdef == 0)
            av1param.EnableCdef = defaultOptHevc.CDEF;
        if (av1param.EnableRestoration == 0)
            av1param.EnableRestoration = defaultOptHevc.LRMode;
        if (av1param.EnableSuperres == 0)
            av1param.EnableSuperres = defaultOptHevc.SRMode;
        if (av1param.InterpFilter == 0)
            av1param.InterpFilter = (defaultOptHevc.InterpFilter == ON) ? MFX_AV1_INTERP_SWITCHABLE : 0;
        if (av1param.DisableFrameEndUpdateCdf == 0)
            av1param.DisableFrameEndUpdateCdf = defaultOptHevc.DisableFrameEndUpdateCdf;
#endif
        if (optHevc.CFLMode == 0)
            optHevc.CFLMode = defaultOptHevc.CFLMode;
        if (optHevc.DisableFrameEndUpdateCdf == 0)
            optHevc.DisableFrameEndUpdateCdf = defaultOptHevc.DisableFrameEndUpdateCdf;
        if (optHevc.NumGpuSlices == 0)
            optHevc.NumGpuSlices = (fi.Width * fi.Height < 3840 * 2160) ? 1 : 2;
    }

    mfxStatus CheckIoPattern(MFXCoreInterface1 &core, const mfxVideoParam &param)
    {
//        if ((param.IOPattern & VIDMEM) && !core.IsExternalFrameAllocator())
//            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (param.IOPattern & OPAQMEM) {
            const mfxExtOpaqueSurfaceAlloc *opaq = GetExtBuffer(param);
            if (opaq == NULL)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            if (!(opaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) &&
                !(opaq->In.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) &&
                !(opaq->In.Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                return MFX_ERR_INVALID_VIDEO_PARAM;
            if (opaq->In.NumSurface == 0 || opaq->In.Surfaces == NULL)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        return MFX_ERR_NONE;
    }

    mfxFrameSurface1 *GetNativeSurface(MFXCoreInterface1 &core, mfxFrameSurface1 *input)
    {
        mfxFrameSurface1 *native = nullptr;
        mfxStatus sts = core.GetRealSurface(input, &native);
        if (sts != MFX_ERR_NONE)
            return nullptr;

        if (native && input && native != input) { // input surface is opaque surface
            native->Data.FrameOrder = input->Data.FrameOrder;
            native->Data.TimeStamp = input->Data.TimeStamp;
            native->Data.Corrupted = input->Data.Corrupted;
            native->Data.DataFlag = input->Data.DataFlag;
            native->Info = input->Info;
        }
        return native;
    }
}; // anonymous namespace


MFXVideoENCODEAV1::MFXVideoENCODEAV1(MFXCoreInterface1 *core, mfxStatus *sts)
    : m_core(core)
{
    if (sts)
        *sts = MFX_ERR_NONE;
}


MFXVideoENCODEAV1::~MFXVideoENCODEAV1()
{
    Close();
}


mfxStatus MFXVideoENCODEAV1::Init(mfxVideoParam * par)
{
    if (par == 0)
        return MFX_ERR_NULL_PTR;

    if (m_impl.get() != 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (CheckExtBuffers(par->ExtParam, par->NumExtParam) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!AllMandatedParamsPresent(par))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    Zero(m_mfxParam);
    m_extBuffers.CleanUp();
    m_mfxParam.NumExtParam = sizeof(m_extBuffers.extParamAll) / sizeof(m_extBuffers.extParamAll[0]);
    m_mfxParam.ExtParam = m_extBuffers.extParamAll;

    CopyParam(m_mfxParam, *par);

    if (m_extBuffers.extOptAv1.EnableCm == 0)
        m_extBuffers.extOptAv1.EnableCm = DEFAULT_ENABLE_CM;

    mfxStatus paramCheckStatus = CheckParam(m_mfxParam);
    if (paramCheckStatus < MFX_ERR_NONE)
        return paramCheckStatus;

    if (CheckIoPattern(*m_core, m_mfxParam) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    SetDefaultValues(m_mfxParam);

    if (m_mfxParam.IOPattern & OPAQMEM)
        if (AllocOpaqSurfaces() != MFX_ERR_NONE)
            return MFX_ERR_MEMORY_ALLOC;

    m_impl.reset(new AV1Encoder(*m_core));
    mfxStatus sts = m_impl->Init(m_mfxParam);
    if (sts != MFX_ERR_NONE) {
        FreeOpaqSurfaces();
        m_impl.reset(NULL);
        return sts;
    }

    m_extBuffers.extSpsPps.SPSBuffer = const_cast<uint8_t*>(m_impl->GetSps(m_extBuffers.extSpsPps.SPSBufSize));

    return paramCheckStatus;
}


mfxStatus MFXVideoENCODEAV1::Close()
{
    if (m_impl.get() == NULL)
        return MFX_ERR_NOT_INITIALIZED;
    FreeOpaqSurfaces();
    m_impl.reset();
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEAV1::Query(MFXCoreInterface1 *core, mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus st = MFX_ERR_NONE;
    if (out == NULL)
        return MFX_ERR_NULL_PTR;

    if (const mfxExtEncoderCapability *inCaps = GetExtBuffer(*in)) {
        // special case: called from MFT
        mfxExtEncoderCapability *outCaps = GetExtBuffer(*out);
        if (outCaps == nullptr)
            return MFX_ERR_UNSUPPORTED;
        //outCaps->MBPerSec = 33 * (1920 * 1088 >> 12); // LCU based
        outCaps->MBPerSec = 38 * (1920 * 1088 >> 8); // 16x16 based (expected > 300k)
        return MFX_ERR_NONE;
    }

    st = CheckExtBuffers(out->ExtParam, out->NumExtParam);
    if (st != MFX_ERR_NONE)
        return st;

    if (in == NULL) { // Query mode1
        MarkConfigurableParams(out);

    } else { // Query mode2
        st = CheckExtBuffers(in->ExtParam, in->NumExtParam);
        if (st != MFX_ERR_NONE)
            return st;

        if (!MatchExtBuffers(*in, *out))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        CopyParam(*out, *in);
        st = CheckParam(*out);

        if (st == MFX_ERR_UNSUPPORTED || st == MFX_ERR_INVALID_VIDEO_PARAM)
            return MFX_ERR_UNSUPPORTED;
        else if (st == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEAV1::QueryIOSurf(MFXCoreInterface1 *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    if (par == 0 || request == 0)
        return MFX_ERR_NULL_PTR;

    if (CheckExtBuffers(par->ExtParam, par->NumExtParam) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!AllMandatedParamsPresent(par))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxVideoParam tmp;
    ExtBuffers tmpExtBuffers;
    Zero(tmp);
    tmpExtBuffers.CleanUp();
    tmp.NumExtParam = sizeof(tmpExtBuffers.extParamAll) / sizeof(tmpExtBuffers.extParamAll[0]);
    tmp.ExtParam = tmpExtBuffers.extParamAll;

    CopyParam(tmp, *par);
    if (CheckParam(tmp) < MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    SetDefaultValues(tmp);

    mfxU16 buffering = tmp.mfx.GopRefDist                 // to reorder B frames
        + (tmpExtBuffers.extOptAv1.FramesInParallel - 1) // for frame-threading
        + (tmp.AsyncDepth - 1);                           // for async mode

    request->NumFrameMin = buffering;
    request->NumFrameSuggested = buffering;
    request->Info = par->mfx.FrameInfo;

    if (par->IOPattern & VIDMEM)
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    else if (par->IOPattern & OPAQMEM)
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;
    else
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEAV1::Reset(mfxVideoParam *par)
{
    if (m_impl.get() == 0)
        return MFX_ERR_NOT_INITIALIZED;

    // Check that only TemporalLayer Params have changed
    if (par && CheckParamHasOnlyTemporalLayerChange(*par, m_mfxParam)) {
        // Check new Params
        ErrorFlag wrnIncompatible = false;
        CopyParam(m_mfxParam, *par);    // safe to copy since everything is the same as before (memcmp based check, no recheck of params)
        mfxExtAvcTemporalLayers *tlayers = GetExtBuffer(m_mfxParam);
        mfxExtCodingOption3 *opt3 = GetExtBuffer(m_mfxParam);
        if (tlayers) {
            if (tlayers->Layer[0].Scale && tlayers->Layer[0].Scale != 1) {
                tlayers->Layer[0].Scale = 1;
                wrnIncompatible = true;
            }

            for (int32_t i = 3; i < 8; i++) {
                if (tlayers->Layer[i].Scale) {
                    tlayers->Layer[i].Scale = 0;
                    wrnIncompatible = true;
                }
            }
            if ((tlayers->Layer[1].Scale || tlayers->Layer[2].Scale) && (m_mfxParam.mfx.GopRefDist != 1 || m_mfxParam.mfx.EncodedOrder || (opt3 && opt3->PRefType == 1))) {
                tlayers->Layer[1].Scale = 0;
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
            if (tlayers->Layer[1].Scale && tlayers->Layer[1].Scale != 2) {
                tlayers->Layer[1].Scale = 0;
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
            if (tlayers->Layer[2].Scale && tlayers->Layer[2].Scale != 4) {
                tlayers->Layer[2].Scale = 0;
                wrnIncompatible = true;
            }
        }
        // Reinit NumRefLayers
        mfxStatus sts = m_impl->ResetTemporalLayers(m_mfxParam);

        if (sts != MFX_ERR_NONE) return sts;
        if (wrnIncompatible) return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        return MFX_ERR_NONE;
    }
    Close();
    return Init(par);
}


mfxStatus MFXVideoENCODEAV1::GetVideoParam(mfxVideoParam *par)
{
    if (m_impl.get() == 0)
        return MFX_ERR_NOT_INITIALIZED;
    if (par == 0)
        return MFX_ERR_NULL_PTR;

    if (mfxExtCodingOptionSPSPPS *spspps = GetExtBuffer(*par)) {
        // check that SPS/PPS buffers have enough space
        if (spspps->SPSBuffer && spspps->SPSBufSize < m_extBuffers.extSpsPps.SPSBufSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        if (spspps->PPSBuffer && spspps->PPSBufSize < m_extBuffers.extSpsPps.PPSBufSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if (mfxExtCodingOptionVPS *vps = GetExtBuffer(*par)) {
        // check that VPS buffer have enough space
        if (vps->VPSBuffer == NULL)
            return MFX_ERR_NULL_PTR;
        if (vps->VPSBufSize < m_extBuffers.extVps.VPSBufSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    CopyParam(*par, m_mfxParam);
    return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEAV1::GetEncodeStat(mfxEncodeStat *stat)
{
    if (m_impl.get() == 0)
        return MFX_ERR_NOT_INITIALIZED;
    if (stat == 0)
        return MFX_ERR_NULL_PTR;

    m_impl->GetEncodeStat(*stat);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEAV1::EncodeFrameCheck(
    mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs,mfxFrameSurface1 **reordered_surface,
    mfxEncodeInternalParams *, MFX_ENTRY_POINT *pEntryPoint)
{
    if (m_impl.get() == 0)
        return MFX_ERR_NOT_INITIALIZED;
    if (pEntryPoint == NULL)
        return MFX_ERR_NULL_PTR;

    if (bs == NULL)
        return MFX_ERR_NULL_PTR;
    uint32_t maxFrameSizeInKB = m_mfxParam.mfx.BufferSizeInKB * MAX(1, m_mfxParam.mfx.BRCParamMultiplier);
    if (bs->MaxLength - bs->DataOffset - bs->DataLength < (int64_t)maxFrameSizeInKB * 1000)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    if (bs->Data == NULL)
        return MFX_ERR_NULL_PTR;
    if (bs->MaxLength < bs->DataOffset + bs->DataLength)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (surface) { // check frame parameters
        if (surface->Info.ChromaFormat != m_mfxParam.mfx.FrameInfo.ChromaFormat)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (surface->Info.Width < m_mfxParam.mfx.FrameInfo.Width)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (surface->Info.Height < m_mfxParam.mfx.FrameInfo.Height)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (surface->Data.Y) {
            if (surface->Data.UV == NULL)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (surface->Data.Pitch >= 0x8000 || surface->Data.Pitch == 0)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        } else if (surface->Data.UV != NULL)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (ctrl) {
        if (!m_mfxParam.mfx.EncodedOrder && ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN)
            if ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_I) &&
                (ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I) &&
                (ctrl->FrameType & 0xff) !=  MFX_FRAMETYPE_IDR)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        if (ctrl->Payload) {
            for (int32_t i = 0; i < ctrl->NumPayload; i++) {
                if (ctrl->Payload[i] == NULL)
                    return MFX_ERR_NULL_PTR;
                if (ctrl->Payload[i]->NumBit > 0 && ctrl->Payload[i]->Data == NULL)
                    return MFX_ERR_NULL_PTR;
                if (ctrl->Payload[i]->NumBit > 8u * ctrl->Payload[i]->BufSize)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
    }

    if (m_mfxParam.mfx.EncodedOrder) {
        if (surface) {
            if (ctrl == NULL)
                return MFX_ERR_NULL_PTR;
            if ((ctrl->FrameType & 7) != MFX_FRAMETYPE_I &&
                (ctrl->FrameType & 7) != MFX_FRAMETYPE_P &&
                (ctrl->FrameType & 7) != MFX_FRAMETYPE_B)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            if (m_mfxParam.mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT) {
                mfxExtLAFrameStatistics *laStat = GetExtBuffer(*ctrl);
                if (laStat == NULL)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
    }

    *reordered_surface = surface;

    mfxFrameSurface1 *native = (m_mfxParam.IOPattern & OPAQMEM) ? GetNativeSurface(*m_core, surface) : surface;
    if (surface && !native)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return m_impl->EncodeFrameCheck(ctrl, native, bs, *pEntryPoint);
}


mfxStatus MFXVideoENCODEAV1::AllocOpaqSurfaces()
{
    mfxFrameAllocRequest request = {};
    request.Info = m_mfxParam.mfx.FrameInfo;
    request.Type = m_extBuffers.extOpaq.In.Type;
    request.NumFrameMin = m_extBuffers.extOpaq.In.NumSurface;
    request.NumFrameSuggested = m_extBuffers.extOpaq.In.NumSurface;

    mfxStatus st = m_core->MapOpaqueSurface(m_extBuffers.extOpaq.In.NumSurface, m_extBuffers.extOpaq.In.Type, m_extBuffers.extOpaq.In.Surfaces);
    if (st != MFX_ERR_NONE)
        return MFX_ERR_MEMORY_ALLOC;

    return MFX_ERR_NONE;
}


void MFXVideoENCODEAV1::FreeOpaqSurfaces()
{
    if (m_core && (m_mfxParam.IOPattern & OPAQMEM)) {
        m_core->UnmapOpaqueSurface(m_extBuffers.extOpaq.In.NumSurface, m_extBuffers.extOpaq.In.Type, m_extBuffers.extOpaq.In.Surfaces);
    }
}

const size_t ExtBuffers::NUM_EXT_PARAM = sizeof(ExtBuffers().extParamAll) / sizeof(ExtBuffers().extParamAll[0]);

ExtBuffers::ExtBuffers()
{
    Zero(extParamAll);
    CleanUp();
    size_t count = 0;
    extParamAll[count++] = &extOpaq.Header;
    extParamAll[count++] = &extOptAv1.Header;
    extParamAll[count++] = &extDumpFiles.Header;
    extParamAll[count++] = &extTiles.Header;
    extParamAll[count++] = &extRegion.Header;
    extParamAll[count++] = &extHevcParam.Header;
    extParamAll[count++] = &extOpt.Header;
    extParamAll[count++] = &extOpt2.Header;
    extParamAll[count++] = &extOpt3.Header;
    extParamAll[count++] = &extSpsPps.Header;
    extParamAll[count++] = &extVps.Header;
    extParamAll[count++] = &extRoi.Header;
    extParamAll[count++] = &extTlayers.Header;
    extParamAll[count++] = &extVP9Param.Header;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    extParamAll[count++] = &extAV1Param.Header;
#endif
    assert(NUM_EXT_PARAM == count);
}

void ExtBuffers::CleanUp()
{
    InitExtBuffer(extOpaq);
    InitExtBuffer(extOptAv1);
    InitExtBuffer(extDumpFiles);
    InitExtBuffer(extTiles);
    InitExtBuffer(extRegion);
    InitExtBuffer(extHevcParam);
    InitExtBuffer(extOpt);
    InitExtBuffer(extOpt2);
    InitExtBuffer(extOpt3);
    InitExtBuffer(extSpsPps);
    InitExtBuffer(extVps);
    InitExtBuffer(extRoi);
    InitExtBuffer(extTlayers);
    InitExtBuffer(extVP9Param);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    InitExtBuffer(extAV1Param);
#endif
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
