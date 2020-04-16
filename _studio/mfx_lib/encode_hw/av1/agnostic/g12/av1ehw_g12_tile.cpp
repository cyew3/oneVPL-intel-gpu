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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_g12_tile.h"
#include <algorithm>
#include <numeric>

using namespace AV1EHW;
using namespace AV1EHW::Gen12;

namespace AV1EHW
{
using TileSizeType = decltype(mfxExtAV1Param::TileWidthInSB[0]);
using TileSizeArrayType = decltype(mfxExtAV1Param::TileWidthInSB);

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
    mfxU32 sbCols
    , mfxU32 sbRows
    , mfxU32 numTileColumns
    , mfxU32 numTileRows
    , const TileSizeArrayType &tileWidthInSB
    , TileLimits& tileLimits)
{
    assert(tileWidthInSB[0] > 0);

    mfxU32 tileAreaSb = sbCols * sbRows;
    mfxU32 TileColsLog2 = TileLog2(mfxU32(1), numTileColumns);
    mfxU32 TileRowsLog2 = TileLog2(mfxU32(1), numTileRows);

    tileLimits.MaxTileWidthSb = CeilDiv(MAX_AV1_TILE_WIDTH, SB_SIZE);
    tileLimits.MinLog2TileCols = TileLog2(tileLimits.MaxTileWidthSb, sbCols);
    tileLimits.MaxLog2TileCols = TileLog2(mfxU32(1), std::min(sbCols, MAX_AV1_NUM_TILE_COLS));
    tileLimits.MaxLog2TileRows = TileLog2(mfxU32(1), std::min(sbRows, MAX_AV1_NUM_TILE_ROWS));
    tileLimits.MaxTileAreaSb = CeilDiv(MAX_AV1_TILE_AREA, mfxU32(SB_SIZE) * SB_SIZE);
    tileLimits.MinLog2Tiles = std::max(tileLimits.MinLog2TileCols,
        TileLog2(tileLimits.MaxTileAreaSb, tileAreaSb));

    CheckRangeOrClip(TileColsLog2, tileLimits.MinLog2TileCols, tileLimits.MaxLog2TileCols);

    tileLimits.MinLog2TileRows = (tileLimits.MinLog2Tiles >= TileColsLog2)
        ? (tileLimits.MinLog2Tiles -TileColsLog2) : 0;

    CheckRangeOrClip(TileRowsLog2, tileLimits.MinLog2TileRows, tileLimits.MaxLog2TileRows);

    mfxU32 widestTileSb = 0;
    std::for_each(tileWidthInSB, tileWidthInSB + numTileColumns,
        [&](mfxU32 x) {
            widestTileSb = std::max(widestTileSb, x);
        });

    if (tileLimits.MinLog2Tiles)
        tileAreaSb >>= (tileLimits.MinLog2Tiles + 1);

    tileLimits.MaxTileHeightSb = std::max(mfxU32(1), tileAreaSb / widestTileSb);
}

inline mfxU32 CheckTileLimits(
    mfxU32 numTileColumns
    , mfxU32 numTileRows
    , const TileSizeArrayType &tileWidthInSB
    , const TileSizeArrayType &tileHeightInSB
    , const TileLimits& tileLimits)
{
    mfxU32 invalid = 0;
    for (mfxU32 i = 0; i < numTileColumns; i++)
        invalid += tileWidthInSB[i] > tileLimits.MaxTileWidthSb;

    for (mfxU32 i = 0; i < numTileRows; i++)
        invalid += tileHeightInSB[i] > tileLimits.MaxTileHeightSb;

    return invalid;
}

inline void CleanTileBuffer(mfxExtAV1Param& av1Par)
{
    std::fill_n(av1Par.TileWidthInSB, av1Par.NumTileColumns, mfxU16(0));
    std::fill_n(av1Par.TileHeightInSB, av1Par.NumTileRows, mfxU16(0));
    std::fill_n(av1Par.NumTilesPerTileGroup, av1Par.NumTileGroups, mfxU16(0));

    av1Par.UniformTileSpacing = 0;
    av1Par.ContextUpdateTileIdPlus1 = 0;
    av1Par.NumTileRows = 0;
    av1Par.NumTileColumns = 0;
    av1Par.NumTileGroups = 0;
}

inline bool InferTileNumber(mfxU16& tileNum, const TileSizeArrayType &tileSizeInSB)
{
    if (tileNum > 0 || tileSizeInSB[0] == 0)
        return false;

    mfxU16 i = 0;
    while (i < sizeof(TileSizeArrayType) / sizeof(TileSizeType) && tileSizeInSB[i] > 0)
        i++;

    tileNum = i;
    return true;
}

mfxStatus CheckAndFixTileBuffer(
    mfxU16 sbCols
    , mfxU16 sbRows
    , mfxExtAV1Param& av1Par)
{
    mfxU32 invalid = 0, changed = 0;

    // Non-uniform tile spacing isn't supported yet
    invalid += IsOff(av1Par.UniformTileSpacing);

    mfxU16 numTileColumns = av1Par.NumTileColumns;
    mfxU16 numTileRows = av1Par.NumTileRows;

    InferTileNumber(numTileColumns, av1Par.TileWidthInSB);
    InferTileNumber(numTileRows, av1Par.TileHeightInSB);

    // NumTileColumns and NumTileRows should be set together
    invalid += (numTileColumns > 0 || numTileRows > 0) && numTileColumns * numTileRows == 0;
    invalid += (numTileColumns > MAX_TILE_COLS);
    invalid += (numTileRows > MAX_TILE_ROWS);

    if (invalid)
    {
        av1Par.NumTileColumns = 0;
        av1Par.NumTileRows = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    // Tile heights/widths should be equal to image heights/widths in SB block
    TileSizeArrayType tileWidthInSB = { 0 };
    TileSizeArrayType tileHeightInSB = { 0 };

    std::copy_n(av1Par.TileWidthInSB, numTileColumns, tileWidthInSB);
    std::copy_n(av1Par.TileHeightInSB, numTileRows, tileHeightInSB);
    if (numTileColumns)
    {
        // Need to calculate tile size if not set for checking if Tile numbers are available
        SetUniformTileSize(sbCols, numTileColumns, tileWidthInSB);
        SetUniformTileSize(sbRows, numTileRows, tileHeightInSB);

        auto widthsSum = std::accumulate(tileWidthInSB, tileWidthInSB + numTileColumns, 0);
        invalid += widthsSum != sbCols;

        auto heightsSum = std::accumulate(tileHeightInSB, tileHeightInSB + numTileRows, 0);
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
        std::fill_n(av1Par.TileWidthInSB, numTileColumns, mfxU16(0));
        std::fill_n(av1Par.TileHeightInSB, numTileRows, mfxU16(0));
        av1Par.NumTileColumns = 0;
        av1Par.NumTileRows = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    /*Issue: cant have default values for missing buffer in current implementation and Check*** are never called
    again after last invocation of SetDefaults.
    */
    mfxU8 numTiles = static_cast<mfxU8>(numTileColumns * numTileRows);
    changed += CheckMaxOrClip(av1Par.ContextUpdateTileIdPlus1, numTiles);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

void Tile::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1Param*)pSrc;
        auto& buf_dst = *(mfxExtAV1Param*)pDst;

        MFX_COPY_FIELD(UniformTileSpacing);
        MFX_COPY_FIELD(ContextUpdateTileIdPlus1);
        MFX_COPY_FIELD(NumTileRows);
        MFX_COPY_FIELD(NumTileColumns);
        MFX_COPY_FIELD(NumTileGroups);

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

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_PARAM].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1Param);

        INHERIT_OPT(UniformTileSpacing);
        INHERIT_OPT(ContextUpdateTileIdPlus1);
        INHERIT_OPT(NumTileRows);
        INHERIT_OPT(NumTileColumns);
        INHERIT_OPT(NumTileGroups);

        for (mfxU32 i = 0; i < sizeof(ebInit.NumTilesPerTileGroup) / sizeof(mfxU16)
            && ebInit.NumTilesPerTileGroup[i] != 0; i++)
        {
            INHERIT_OPT(NumTilesPerTileGroup[i]);
        }

        for (mfxU32 i = 0; i < sizeof(ebInit.TileHeightInSB) / sizeof(mfxU16)
            && ebInit.TileHeightInSB[i] != 0; i++)
        {
            INHERIT_OPT(TileHeightInSB[i]);
        }

        for (mfxU32 i = 0; i < sizeof(ebInit.TileWidthInSB) / sizeof(mfxU16)
            && ebInit.TileWidthInSB[i] != 0; i++)
        {
            INHERIT_OPT(TileWidthInSB[i]);
        }
    });
}

void Tile::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        mfxExtAV1Param* pAV1Par = ExtBuffer::Get(out);
        MFX_CHECK(pAV1Par, MFX_ERR_NONE);

        const mfxU16 frameWidth = pAV1Par->FrameWidth ? pAV1Par->FrameWidth : out.mfx.FrameInfo.Width;
        const mfxU16 frameHeight = pAV1Par->FrameHeight ? pAV1Par->FrameHeight : out.mfx.FrameInfo.Height;
        const mfxU16 sbCols = CeilDiv<mfxU16>(frameWidth, SB_SIZE);
        const mfxU16 sbRows = CeilDiv<mfxU16>(frameHeight, SB_SIZE);

        return CheckAndFixTileBuffer(sbCols, sbRows, *pAV1Par);
    });
}

inline void SetDefaultTileSize(const mfxU16 numSB, mfxU16& numTile, TileSizeArrayType &tileSizeInSB)
{
    if (numTile == 0)
    {
        numTile = 1;
        tileSizeInSB[0] = numSB;
    }
    else
        SetUniformTileSize(numSB, numTile, tileSizeInSB);
}

inline void SetDefaultTileParams(
    mfxU16 sbCols
    , mfxU16 sbRows
    , mfxExtAV1Param& av1Par)
{
    SetDefault(av1Par.UniformTileSpacing, MFX_CODINGOPTION_ON);

    InferTileNumber(av1Par.NumTileColumns, av1Par.TileWidthInSB);
    InferTileNumber(av1Par.NumTileRows, av1Par.TileHeightInSB);

    SetDefaultTileSize(sbCols, av1Par.NumTileColumns, av1Par.TileWidthInSB);
    SetDefaultTileSize(sbRows, av1Par.NumTileRows, av1Par.TileHeightInSB);

    // Use last Tile in default mode
    av1Par.ContextUpdateTileIdPlus1 = (av1Par.ContextUpdateTileIdPlus1 == 0) ?
        mfxU8(av1Par.NumTileColumns * av1Par.NumTileRows) : av1Par.ContextUpdateTileIdPlus1;
}

void Tile::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
        if (!pAV1Par)
            return;

        const mfxU16 sbCols = CeilDiv<mfxU16>(pAV1Par->FrameWidth, SB_SIZE);
        const mfxU16 sbRows = CeilDiv<mfxU16>(pAV1Par->FrameHeight, SB_SIZE);
        SetDefaultTileParams(sbCols, sbRows, *pAV1Par);
    });
}

inline void SetTileInfo(
    mfxU16 sbCols
    , mfxU16 sbRows
    , const mfxExtAV1Param& av1Par
    , const EncodeCapsAv1& caps
    , TileInfo& tileInfo)
{
    tileInfo.TileCols = av1Par.NumTileColumns;
    tileInfo.TileRows = av1Par.NumTileRows;
    tileInfo.TileColsLog2 = TileLog2(mfxU32(1), mfxU32(av1Par.NumTileColumns));
    tileInfo.TileRowsLog2 = TileLog2(mfxU32(1), mfxU32(av1Par.NumTileRows));

    SetTileLimits(sbCols, sbRows, av1Par.NumTileColumns, av1Par.NumTileRows, av1Par.TileWidthInSB, tileInfo.tileLimits);

    for (int i = 0; i < av1Par.NumTileColumns; i++)
        tileInfo.TileWidthInSB[i] = av1Par.TileWidthInSB[i];

    for (int i = 0; i < av1Par.NumTileRows; i++)
        tileInfo.TileHeightInSB[i] = av1Par.TileHeightInSB[i];

    tileInfo.uniform_tile_spacing_flag = CO2Flag(av1Par.UniformTileSpacing);
    tileInfo.context_update_tile_id = av1Par.ContextUpdateTileIdPlus1 - 1;

    tileInfo.TileSizeBytes = caps.tile_size_bytes_minus_1 + 1;
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

void Tile::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetTileInfo
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(par);
        MFX_CHECK(pAV1Par, MFX_ERR_UNDEFINED_BEHAVIOR);

        //In current design FH should be initialized in General feature first
        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(strg.Contains(Glob::EncodeCaps::Key), MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& fh = Glob::FH::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);

        mfxU16 sbCols = CeilDiv<mfxU16>(pAV1Par->FrameWidth , SB_SIZE);
        mfxU16 sbRows = CeilDiv<mfxU16>(pAV1Par->FrameHeight, SB_SIZE);
        fh.sbCols = sbCols;
        fh.sbRows = sbRows;
        SetTileInfo(sbCols, sbRows, *pAV1Par, caps, fh.tile_info);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetTileGroups
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        const mfxExtAV1Param& av1Par = ExtBuffer::Get(par); // all ext buffers must be present at this stage

        auto& fh = Glob::FH::Get(strg);
        mfxU32 numTiles = fh.tile_info.TileCols * fh.tile_info.TileRows;

        auto& tileGroupInfos = Glob::TileGroups::GetOrConstruct(strg);
        SetTileGroupsInfo(av1Par.NumTileGroups, numTiles, tileGroupInfos);

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
            task.Insert(Task::TileGroups::Key, new MakeStorable<Task::TileGroups::TRef>);
            return MFX_ERR_NONE;
        });
}

inline bool IsTileUpdated(mfxExtAV1Param *pFrameAv1Par)
{
    return pFrameAv1Par && (pFrameAv1Par->NumTileColumns > 0 || pFrameAv1Par->NumTileRows > 0
        || pFrameAv1Par->TileWidthInSB[0] > 0 || pFrameAv1Par->TileHeightInSB[0] > 0);
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
            mfxExtAV1Param *pFrameAv1Par = ExtBuffer::Get(Task::Common::Get(task).ctrl);
            MFX_CHECK(IsTileUpdated(pFrameAv1Par), MFX_ERR_NONE);

            const auto& par = Glob::VideoParam::Get(global);
            const mfxExtAV1Param& av1Par = ExtBuffer::Get(par);
            const mfxU16 sbCols = CeilDiv<mfxU16>(av1Par.FrameWidth, SB_SIZE);
            const mfxU16 sbRows = CeilDiv<mfxU16>(av1Par.FrameHeight, SB_SIZE);

            mfxStatus sts = CheckAndFixTileBuffer(sbCols, sbRows, *pFrameAv1Par);
            if (sts < MFX_ERR_NONE)
            {
                // Ignore Frame Tile param and return warning if there are issues MSDK can't fix
                AV1E_LOG("  *DEBUG* STATUS: Critical issue in Frame tile settings - it's ignored!\n");
                CleanTileBuffer(*pFrameAv1Par);
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            SetDefaultTileParams(sbCols, sbRows, *pFrameAv1Par);
            return sts;
        });
}

void Tile::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        mfxExtAV1Param *pFrameAv1Par = ExtBuffer::Get(Task::Common::Get(s_task).ctrl);
        if (!IsTileUpdated(pFrameAv1Par))
        {
            Task::TileGroups::Get(s_task) = {};
            return MFX_ERR_NONE;
        }

        const auto& par = Glob::VideoParam::Get(global);
        const mfxExtAV1Param& av1Par = ExtBuffer::Get(par);
        const mfxU16 sbCols = CeilDiv<mfxU16>(av1Par.FrameWidth, SB_SIZE);
        const mfxU16 sbRows = CeilDiv<mfxU16>(av1Par.FrameHeight, SB_SIZE);

        const auto& caps = Glob::EncodeCaps::Get(global);
        auto& fh = Task::FH::Get(s_task);
        SetTileInfo(sbCols, sbRows, *pFrameAv1Par, caps, fh.tile_info);

        mfxU32 numTiles = fh.tile_info.TileCols * fh.tile_info.TileRows;
        auto& tileGroupInfos = Task::TileGroups::Get(s_task);
        SetTileGroupsInfo(pFrameAv1Par->NumTileGroups, numTiles, tileGroupInfos);

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
