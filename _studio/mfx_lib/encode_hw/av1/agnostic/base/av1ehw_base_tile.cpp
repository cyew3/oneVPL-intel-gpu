// Copyright (c) 2020-2021 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_tile.h"
#include <algorithm>
#include <numeric>

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace AV1EHW
{
using TileSizeType = decltype(mfxExtAV1AuxData::TileWidthInSB[0]);
using TileSizeArrayType = decltype(mfxExtAV1AuxData::TileWidthInSB);

inline void SetUniformTileSize(
    const mfxU16 sbNum
    , const mfxU16 tileNum
    , TileSizeArrayType& tileSizeInSB)
{
    if (tileNum == 0 || tileSizeInSB[0] != 0)
        return;

    mfxU16 log2TileSize = TileLog2(mfxU16(1), tileNum);
    mfxU16 t = (sbNum + (1 << log2TileSize) - 1) >> log2TileSize;
    mfxU16 cnt = sbNum / t;

    assert(cnt <= tileNum);

    std::fill_n(tileSizeInSB, cnt, t);

    if (cnt * t == sbNum)
        return;

    if(cnt < tileNum)
        tileSizeInSB[cnt] = sbNum - cnt * t;
    else
        tileSizeInSB[cnt - 1] += sbNum - cnt * t;
}

inline void SetTileLimits(
    mfxU16 sbCols
    , mfxU16 sbRows
    , mfxU32 numTileColumns
    , mfxU32 numTileRows
    , const TileSizeArrayType &tileWidthInSB
    , TileLimits& tileLimits)
{
    assert(tileWidthInSB[0] > 0);

    mfxU32 tileAreaSb   = sbCols * sbRows;
    mfxU32 TileColsLog2 = TileLog2(mfxU32(1), numTileColumns);
    mfxU32 TileRowsLog2 = TileLog2(mfxU32(1), numTileRows);

    tileLimits.MaxTileWidthSb  = mfx::CeilDiv(MAX_AV1_TILE_WIDTH, SB_SIZE);
    tileLimits.MinLog2TileCols = TileLog2(tileLimits.MaxTileWidthSb, mfxU32(sbCols));
    tileLimits.MaxLog2TileCols = TileLog2(mfxU32(1), std::min(mfxU32(sbCols), MAX_AV1_NUM_TILE_COLS));
    tileLimits.MaxLog2TileRows = TileLog2(mfxU32(1), std::min(mfxU32(sbRows), MAX_AV1_NUM_TILE_ROWS));
    tileLimits.MaxTileAreaSb   = mfx::CeilDiv(MAX_AV1_TILE_AREA, SB_SIZE * SB_SIZE);
    tileLimits.MinLog2Tiles    = std::max(tileLimits.MinLog2TileCols,
        TileLog2(tileLimits.MaxTileAreaSb, tileAreaSb));

    CheckRangeOrClip(TileColsLog2, tileLimits.MinLog2TileCols, tileLimits.MaxLog2TileCols);

    tileLimits.MinLog2TileRows = (tileLimits.MinLog2Tiles >= TileColsLog2)
        ? (tileLimits.MinLog2Tiles -TileColsLog2) : 0;

    CheckRangeOrClip(TileRowsLog2, tileLimits.MinLog2TileRows, tileLimits.MaxLog2TileRows);

    if (tileLimits.MinLog2Tiles)
        tileAreaSb >>= (tileLimits.MinLog2Tiles + 1);

    mfxU32 widestTileSb = 0;
    std::for_each(tileWidthInSB, tileWidthInSB + numTileColumns,
        [&](mfxU32 x) {
            widestTileSb = std::max(widestTileSb, x);
        });

    if (widestTileSb != 0)
        tileLimits.MaxTileHeightSb = std::max(mfxU32(1), tileAreaSb / widestTileSb);
}

inline mfxU32 CheckTileLimits(
    mfxU32 numTileColumns
    , mfxU32 numTileRows
    , const TileSizeArrayType& tileWidthInSB
    , const TileSizeArrayType& tileHeightInSB
    , const TileLimits& tileLimits)
{
    mfxU32 invalid = 0;
    for (mfxU32 i = 0; i < numTileColumns; i++)
        invalid += tileWidthInSB[i] > tileLimits.MaxTileWidthSb;

    for (mfxU32 i = 0; i < numTileRows; i++)
        invalid += tileHeightInSB[i] > tileLimits.MaxTileHeightSb;

    return invalid;
}

inline void CleanTileBuffers(mfxExtAV1TileParam* pTilePar, mfxExtAV1AuxData* pAuxPar)
{
    if (pTilePar)
    {
        pTilePar->NumTileRows    = 0;
        pTilePar->NumTileColumns = 0;
        pTilePar->NumTileGroups  = 0;
    }

    if (pAuxPar)
    {
        pAuxPar->UniformTileSpacing       = 0;
        pAuxPar->ContextUpdateTileIdPlus1 = 0;
        std::fill_n(pAuxPar->TileWidthInSB, sizeof(mfxExtAV1AuxData::TileWidthInSB) / sizeof(mfxU16), mfxU16(0));
        std::fill_n(pAuxPar->TileHeightInSB, sizeof(mfxExtAV1AuxData::TileHeightInSB) / sizeof(mfxU16), mfxU16(0));
        std::fill_n(pAuxPar->NumTilesPerTileGroup, sizeof(mfxExtAV1AuxData::NumTilesPerTileGroup) / sizeof(mfxU16), mfxU16(0));
    }
}

inline bool InferTileNumber(mfxU16& tileNum, const TileSizeArrayType& tileSizeInSB)
{
    if (tileNum > 0 || tileSizeInSB[0] == 0)
        return false;

    mfxU16 i = 0;
    while (i < sizeof(TileSizeArrayType) / sizeof(TileSizeType) && tileSizeInSB[i] > 0)
        i++;

    tileNum = i;
    return true;
}

inline void SetDefaultTileParams(
    mfxU16 sbCols
    , mfxU16 sbRows
    , mfxExtAV1TileParam& tilePar
    , mfxExtAV1AuxData* pAuxPar)
{
    if (pAuxPar)
    {
        InferTileNumber(tilePar.NumTileColumns, pAuxPar->TileWidthInSB);
        InferTileNumber(tilePar.NumTileRows, pAuxPar->TileHeightInSB);

        SetDefault(tilePar.NumTileColumns, 1);
        SetDefault(tilePar.NumTileRows, 1);

        SetDefault(pAuxPar->UniformTileSpacing, MFX_CODINGOPTION_ON);
        SetUniformTileSize(sbCols, tilePar.NumTileColumns, pAuxPar->TileWidthInSB);
        SetUniformTileSize(sbRows, tilePar.NumTileRows, pAuxPar->TileHeightInSB);

        // Use last Tile in default mode
        pAuxPar->ContextUpdateTileIdPlus1 = (pAuxPar->ContextUpdateTileIdPlus1 == 0) ? 
            mfxU8(tilePar.NumTileColumns * tilePar.NumTileRows) : pAuxPar->ContextUpdateTileIdPlus1;
    }
    else
    {
        SetDefault(tilePar.NumTileColumns, 1);
        SetDefault(tilePar.NumTileRows, 1);
    }

    SetDefault(tilePar.NumTileGroups, 1);
}

mfxStatus CheckAndFixTileBuffers(
    mfxU16 sbCols
    , mfxU16 sbRows
    , mfxExtAV1TileParam& tilePar
    , mfxExtAV1AuxData& auxPar)
{
    mfxU32 invalid = 0;

    // Non-uniform tile spacing isn't supported yet
    invalid += IsOff(auxPar.UniformTileSpacing);

    // Here we only check, values will be updated in SetDefaults routine
    mfxU16 numTileColumns = tilePar.NumTileColumns;
    mfxU16 numTileRows    = tilePar.NumTileRows;

    InferTileNumber(numTileColumns, auxPar.TileWidthInSB);
    InferTileNumber(numTileRows, auxPar.TileHeightInSB);

    // NumTileColumns and NumTileRows should be set together
    invalid += (numTileColumns > 0 || numTileRows > 0) && numTileColumns * numTileRows == 0;
    invalid += (numTileColumns > MAX_TILE_COLS);
    invalid += (numTileRows > MAX_TILE_ROWS);
    invalid += (numTileColumns > sbCols);
    invalid += (numTileRows > sbRows);

    if (invalid)
    {
        tilePar.NumTileColumns   = 0;
        tilePar.NumTileRows      = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    // Tile heights/widths should be equal to image heights/widths in SB block
    TileSizeArrayType tileWidthInSB  = { 0 };
    TileSizeArrayType tileHeightInSB = { 0 };

    std::copy_n(auxPar.TileWidthInSB, numTileColumns, tileWidthInSB);
    std::copy_n(auxPar.TileHeightInSB, numTileRows, tileHeightInSB);
    if (numTileColumns)
    {
        // Need to calculate tile size if not set for checking if Tile numbers are available
        SetUniformTileSize(sbCols, numTileColumns, tileWidthInSB);
        SetUniformTileSize(sbRows, numTileRows, tileHeightInSB);

        auto widthsSum = std::accumulate(tileWidthInSB, tileWidthInSB + numTileColumns, mfxU32(0));
        invalid += widthsSum != sbCols;

        auto heightsSum = std::accumulate(tileHeightInSB, tileHeightInSB + numTileRows, mfxU32(0));
        invalid += heightsSum != sbRows;
    }

    // Check against tile size limitations defined in AV1 spec when Tile number greater than 1
    if (numTileColumns * numTileRows > 1)
    {
        TileLimits tileLimits{};
        SetTileLimits(sbCols, sbRows, numTileColumns, numTileRows, tileWidthInSB, tileLimits);

        invalid += CheckTileLimits(numTileColumns, numTileRows, tileWidthInSB, tileHeightInSB, tileLimits);
    }

    if (invalid)
    {
        CleanTileBuffers(&tilePar, &auxPar);
        return MFX_ERR_UNSUPPORTED;
    }

    mfxU8 changed  = 0;
    mfxU8 numTiles = static_cast<mfxU8>(numTileColumns * numTileRows);
    changed += CheckMaxOrClip(auxPar.ContextUpdateTileIdPlus1, numTiles);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

inline std::pair<mfxU16, mfxU16> GetSBNum(const mfxVideoParam& vp)
{
    mfxU32 width = 0, height = 0;
    std::tie(width, height) = GetRealResolution(vp);

    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(vp);
    width                           = GetActualEncodeWidth(width, pAuxPar);
    mfxU16                  sbCols  = static_cast<mfxU16>(mfx::CeilDiv(width, SB_SIZE));
    mfxU16                  sbRows  = static_cast<mfxU16>(mfx::CeilDiv(height, SB_SIZE));

    return std::make_pair(sbCols, sbRows);
}

inline void SetTileInfo(
    mfxU16 sbCols
    , mfxU16 sbRows
    , const mfxExtAV1TileParam& tilePar
    , const mfxExtAV1AuxData& auxPar
    , const EncodeCapsAv1& caps
    , TileInfoAv1& tileInfo)
{
    tileInfo = {};

    tileInfo.TileCols     = tilePar.NumTileColumns;
    tileInfo.TileRows     = tilePar.NumTileRows;
    tileInfo.TileColsLog2 = TileLog2(mfxU32(1), mfxU32(tilePar.NumTileColumns));
    tileInfo.TileRowsLog2 = TileLog2(mfxU32(1), mfxU32(tilePar.NumTileRows));

    SetTileLimits(sbCols, sbRows, tilePar.NumTileColumns, tilePar.NumTileRows, auxPar.TileWidthInSB, tileInfo.tileLimits);

    for (int i = 0; i < tilePar.NumTileColumns; i++)
        tileInfo.TileWidthInSB[i] = auxPar.TileWidthInSB[i];

    for (int i = 0; i < tilePar.NumTileRows; i++)
        tileInfo.TileHeightInSB[i] = auxPar.TileHeightInSB[i];

    tileInfo.uniform_tile_spacing_flag = CO2Flag(auxPar.UniformTileSpacing);
    tileInfo.context_update_tile_id    = auxPar.ContextUpdateTileIdPlus1 - 1;
    tileInfo.TileSizeBytes             = caps.TileSizeBytesMinus1 + 1;
}

inline void SetTileGroupsInfo(
    const mfxU32 numTileGroups
    , const mfxU32 numTiles
    , TileGroupInfos& infos)
{
    infos.clear();

    if (numTiles == 0)
    {
        infos.push_back({ 0, 0 });
        return;
    }

    if (numTileGroups <= 1 || numTileGroups > numTiles)
    {
        // several tiles in one tile group
        infos.push_back({0, numTiles - 1});
        return;
    }

    // several tiles in several tile groups
    uint32_t cnt = 0;
    uint32_t size = numTiles / numTileGroups;
    for (uint16_t i = 0; i < numTileGroups; i++)
    {
        TileGroupInfo tgi = { cnt, cnt + size - 1};
        cnt += size;

        if (i == numTileGroups - 1)
            tgi.TgEnd = numTiles - 1;

        infos.push_back(std::move(tgi));
    }
}

void Tile::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_TILE_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1TileParam*)pSrc;
        auto& buf_dst = *(mfxExtAV1TileParam*)pDst;
        MFX_COPY_FIELD(NumTileRows);
        MFX_COPY_FIELD(NumTileColumns);
        MFX_COPY_FIELD(NumTileGroups);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_AUXDATA].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1AuxData*)pSrc;
        auto& buf_dst = *(mfxExtAV1AuxData*)pDst;

        MFX_COPY_FIELD(UniformTileSpacing);
        MFX_COPY_FIELD(ContextUpdateTileIdPlus1);

        for (mfxU32 i = 0; i < sizeof(buf_src.NumTilesPerTileGroup) / sizeof(mfxU16)
            && buf_src.NumTilesPerTileGroup[i] != 0; i++)
        {
            MFX_COPY_FIELD(NumTilesPerTileGroup[i]);
        }

        for (mfxU32 i = 0; i < sizeof(buf_src.TileHeightInSB) / sizeof(mfxU16)
            && buf_src.TileHeightInSB[i] != 0; i++)
        {
            MFX_COPY_FIELD(TileHeightInSB[i]);
        }

        for (mfxU32 i = 0; i < sizeof(buf_src.TileWidthInSB) / sizeof(mfxU16)
            && buf_src.TileWidthInSB[i] != 0; i++)
        {
            MFX_COPY_FIELD(TileWidthInSB[i]);
        }
    });
}

void Tile::SetInherited(ParamInheritance& par)
{
#define INIT_EB(TYPE)\
    if (!pSrc || !pDst) return;\
    auto& ebInit = *(TYPE*)pSrc;\
    auto& ebReset = *(TYPE*)pDst;
#define INHERIT_OPT(OPT) InheritOption(ebInit.OPT, ebReset.OPT);

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_TILE_PARAM].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1TileParam);

        INHERIT_OPT(NumTileRows);
        INHERIT_OPT(NumTileColumns);
        INHERIT_OPT(NumTileGroups);
    });
}

void Tile::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        // Not check any tile related params in AuxData if pTilePar is nullptr
        mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(par);
        if (!pTilePar)
            return;

        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(par);

        mfxExtAV1AuxData* pAuxPar   = ExtBuffer::Get(par);
        SetDefaultTileParams(sbCols, sbRows, *pTilePar, pAuxPar);
    });
}

void Tile::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        // Not checking any tile related params in AuxData if pTilePar is nullptr
        mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(out);
        MFX_CHECK(pTilePar, MFX_ERR_NONE);

        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(out);

        mfxExtAV1AuxData* pAuxPar    = ExtBuffer::Get(out);
        mfxExtAV1AuxData  tempAuxPar = {};
        if (!pAuxPar)
            pAuxPar = &tempAuxPar;

        return CheckAndFixTileBuffers(sbCols, sbRows, *pTilePar, *pAuxPar);
    });
}

void Tile::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetTileInfo
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        //In current design FH should be initialized in General feature first
        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(strg.Contains(Glob::EncodeCaps::Key), MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& fh   = Glob::FH::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);

        auto& par  = Glob::VideoParam::Get(strg);
        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(par);

        fh.sbCols = sbCols;
        fh.sbRows = sbRows;

        const mfxExtAV1TileParam& tilePar = ExtBuffer::Get(par);
        const mfxExtAV1AuxData&   auxPar  = ExtBuffer::Get(par);
        SetTileInfo(sbCols, sbRows, tilePar, auxPar, caps, fh.tile_info);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetTileGroups
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto&  par      = Glob::VideoParam::Get(strg);
        auto&  fh       = Glob::FH::Get(strg);
        mfxU32 numTiles = fh.tile_info.TileCols * fh.tile_info.TileRows;

        const mfxExtAV1TileParam& tilePar        = ExtBuffer::Get(par);
        auto&                     tileGroupInfos = Glob::TileGroups::GetOrConstruct(strg);
        SetTileGroupsInfo(tilePar.NumTileGroups, numTiles, tileGroupInfos);

        return MFX_ERR_NONE;
    });
}

void Tile::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR&
            , StorageRW& task) -> mfxStatus
        {
            task.Insert(Task::TileInfo::Key, new MakeStorable<Task::TileInfo::TRef>);
            task.Insert(Task::TileGroups::Key, new MakeStorable<Task::TileGroups::TRef>);
            return MFX_ERR_NONE;
        });
}

inline bool IsTileUpdated(const mfxExtAV1TileParam* pFrameTilePar)
{
    return pFrameTilePar && (pFrameTilePar->NumTileColumns > 0 || pFrameTilePar->NumTileRows > 0);
}

void Tile::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& task) -> mfxStatus
        {
            mfxExtAV1TileParam* pFrameTilePar = ExtBuffer::Get(Task::Common::Get(task).ctrl);
            MFX_CHECK(IsTileUpdated(pFrameTilePar), MFX_ERR_NONE);

            const auto&               par      = Glob::VideoParam::Get(global);
            const mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(par);
            const mfxExtAV1AuxData*   pAuxPar  = ExtBuffer::Get(par);
            MFX_CHECK(pTilePar && pAuxPar, MFX_ERR_UNDEFINED_BEHAVIOR);

            mfxU16 sbCols = 0, sbRows = 0;
            std::tie(sbCols, sbRows)  = GetSBNum(par);

            mfxExtAV1AuxData* pFrameAuxPar = ExtBuffer::Get(Task::Common::Get(task).ctrl);
            mfxExtAV1AuxData  tempAuxPar   = {};
            if (!pFrameAuxPar)
                pFrameAuxPar = &tempAuxPar;

            SetDefaultTileParams(sbCols, sbRows, *pFrameTilePar, pFrameAuxPar);
            mfxStatus sts = CheckAndFixTileBuffers(sbCols, sbRows, *pFrameTilePar, *pFrameAuxPar);
            if (sts < MFX_ERR_NONE)
            {
                // Ignore Frame Tile param and return warning if there are issues MSDK can't fix
                AV1E_LOG("  *DEBUG* STATUS: Critical issue in Frame tile settings - it's ignored!\n");
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            const auto& caps     = Glob::EncodeCaps::Get(global);
            auto&       tileInfo = Task::TileInfo::Get(task);
            SetTileInfo(sbCols, sbRows, *pFrameTilePar, *pFrameAuxPar, caps, tileInfo);

            return sts;
        });
}

inline void UpdateFrameTileInfo(FH& currFH, const TileInfoAv1& tileInfo)
{
    currFH.tile_info = tileInfo;
}

void Tile::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this, &blocks](
            StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
    {
        mfxExtAV1TileParam* pFrameTilePar = ExtBuffer::Get(Task::Common::Get(s_task).ctrl);
        if (!IsTileUpdated(pFrameTilePar))
        {
            Task::TileGroups::Get(s_task) = {};
            return MFX_ERR_NONE;
        }

        auto& fh        = Task::FH::Get(s_task);
        auto& tileInfo  = Task::TileInfo::Get(s_task);
        UpdateFrameTileInfo(fh, tileInfo);

        mfxU32 numTiles       = pFrameTilePar->NumTileColumns * pFrameTilePar->NumTileColumns;
        auto&  tileGroupInfos = Task::TileGroups::Get(s_task);
        SetTileGroupsInfo(pFrameTilePar->NumTileGroups, numTiles, tileGroupInfos);

        return MFX_ERR_NONE;
    });
}

void Tile::ResetState(const FeatureBlocks& blocks, TPushRS Push)
{
    Push(BLK_ResetState
        , [this, &blocks](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& real = Glob::RealState::Get(global);
        Glob::TileGroups::Get(real) = Glob::TileGroups::Get(global);

        return MFX_ERR_NONE;
    });
}
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
