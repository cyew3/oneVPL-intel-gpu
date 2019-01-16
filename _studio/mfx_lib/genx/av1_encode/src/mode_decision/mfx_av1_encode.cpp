// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfx_av1_defs.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_frame.h"

namespace H265Enc {
    // Find smallest k>=0 such that (blk_size << k) >= target
    int32_t TileLog2(int32_t blk_size, int32_t target) {
        int32_t k;
        for (k = 0; (blk_size << k) < target; k++);
        return k;
    }

    void SetupTileParamAv1(H265VideoParam *par, int32_t numTileRows, int32_t numTileCols, int32_t uniform)
    {
        TileParam *tp = &par->tileParam;
        const int32_t miRows = par->miRows;
        const int32_t miCols = par->miCols;
        const int32_t sbCols = (miCols + 7) >> 3;
        const int32_t sbRows = (miRows + 7) >> 3;
        const int32_t sbSizeLog2 = 6;
        int32_t maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSizeLog2);

        tp->uniformSpacing = uniform;
        tp->cols = numTileCols;
        tp->rows = numTileRows;
        tp->log2Cols = TileLog2(1, numTileCols);
        tp->log2Rows = TileLog2(1, numTileRows);
        tp->maxTileWidthSb = MAX_TILE_WIDTH >> sbSizeLog2;
        tp->minLog2Cols = TileLog2(tp->maxTileWidthSb, sbCols);
        tp->maxLog2Cols = TileLog2(1, IPP_MIN(sbCols, MAX_TILE_COLS));
        tp->maxLog2Rows = TileLog2(1, IPP_MIN(sbRows, MAX_TILE_ROWS));
        tp->minLog2Tiles = TileLog2(maxTileAreaSb, sbCols * sbRows);
        tp->minLog2Tiles = IPP_MAX(tp->minLog2Tiles, tp->minLog2Cols);

        if (uniform) {
            const int32_t tileWidth  = (sbCols + tp->cols - 1) >> tp->log2Cols;
            const int32_t tileHeight = (sbRows + tp->rows - 1) >> tp->log2Rows;

            for (int32_t i = 0; i < tp->cols; i++) {
                tp->colStart[i] = i * tileWidth;
                tp->colEnd[i]   = IPP_MIN(tp->colStart[i] + tileWidth, sbCols);
                tp->colWidth[i] = tp->colEnd[i] - tp->colStart[i];
            }

            tp->minLog2Rows = IPP_MAX(tp->minLog2Tiles - tp->log2Cols, 0);
            tp->maxTileHeightSb = sbRows >> tp->minLog2Rows;

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = i * tileHeight;
                tp->rowEnd[i]    = IPP_MIN(tp->rowStart[i] + tileHeight, sbRows);
                tp->rowHeight[i] = tp->rowEnd[i] - tp->rowStart[i];
            }
        } else {
            int32_t widestTileWidth = 0;
            for (int32_t i = 0; i < tp->cols; i++) {
                tp->colStart[i] = (i + 0) * sbCols / tp->cols;
                tp->colEnd[i]   = (i + 1) * sbCols / tp->cols;
                tp->colWidth[i] = tp->colEnd[i] - tp->colStart[i];
                widestTileWidth = IPP_MAX(widestTileWidth, tp->colWidth[i]);
            }

            if (tp->minLog2Tiles)
                maxTileAreaSb >>= (tp->minLog2Tiles + 1);
            tp->maxTileHeightSb = IPP_MAX(1, maxTileAreaSb / widestTileWidth);

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = (i + 0) * sbRows / tp->rows;
                tp->rowEnd[i]    = (i + 1) * sbRows / tp->rows;
                tp->rowHeight[i] = tp->rowEnd[i] - tp->rowStart[i];
            }
        }

        for (int32_t i = 0; i < tp->cols; i++) {
            // 8x8 units
            tp->miColStart[i] = tp->colStart[i] << 3;
            tp->miColEnd[i]   = IPP_MIN(tp->colEnd[i] << 3, miCols);
            tp->miColWidth[i] = tp->miColEnd[i] - tp->miColStart[i];

            for (int32_t j = tp->colStart[i]; j < tp->colEnd[i]; j++)
                tp->mapSb2TileCol[j] = i;
        }

        for (int32_t i = 0; i < tp->rows; i++) {
            // 8x8 units
            tp->miRowStart[i]  = tp->rowStart[i] << 3;
            tp->miRowEnd[i]    = IPP_MIN(tp->rowEnd[i] << 3, miRows);
            tp->miRowHeight[i] = tp->miRowEnd[i] - tp->miRowStart[i];

            for (int32_t j = tp->rowStart[i]; j < tp->rowEnd[i]; j++)
                tp->mapSb2TileRow[j] = i;
        }
    }

    void SetupTileParamAv1EqualSize(H265VideoParam *par, int32_t tileWidth, int32_t tileHeight)
    {
        TileParam *tp = &par->tileParam;
        const int32_t miRows = par->miRows;
        const int32_t miCols = par->miCols;
        const int32_t sbCols = (miCols + 7) >> 3;
        const int32_t sbRows = (miRows + 7) >> 3;
        const int32_t sbSizeLog2 = 6;
        int32_t maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSizeLog2);

        tp->uniformSpacing = 0;
        tp->cols = 0;
        tp->rows = 0;

        int32_t widestTileWidth = tileWidth;

        if (!tp->uniformSpacing) {
            int32_t start = 0;
            for (int32_t i = 0; i < CodecLimits::MAX_NUM_TILE_COLS && start < sbCols; i++) {
                tp->colStart[i] = start;
                tp->colWidth[i] = IPP_MIN(tileWidth, sbCols - start);
                tp->colEnd[i]   = start + tp->colWidth[i];
                widestTileWidth = IPP_MAX(widestTileWidth, tp->colWidth[i]);
                start += tp->colWidth[i];
                tp->cols++;
            }
            assert(start == sbCols);

            start = 0;
            for (int32_t i = 0; i < CodecLimits::MAX_NUM_TILE_ROWS && start < sbRows; i++) {
                tp->rowStart[i]  = start;
                tp->rowHeight[i] = IPP_MIN(tileHeight, sbRows - start);
                tp->rowEnd[i]    = start + tp->rowHeight[i];
                start += tp->rowHeight[i];
                tp->rows++;
            }
            assert(start == sbRows);
        }

        tp->log2Cols = TileLog2(1, tp->cols);
        tp->log2Rows = TileLog2(1, tp->rows);
        tp->maxTileWidthSb = MAX_TILE_WIDTH >> sbSizeLog2;
        tp->minLog2Cols = TileLog2(tp->maxTileWidthSb, sbCols);
        tp->maxLog2Cols = TileLog2(1, IPP_MIN(sbCols, MAX_TILE_COLS));
        tp->maxLog2Rows = TileLog2(1, IPP_MIN(sbRows, MAX_TILE_ROWS));
        tp->minLog2Tiles = TileLog2(maxTileAreaSb, sbCols * sbRows);
        tp->minLog2Tiles = IPP_MAX(tp->minLog2Tiles, tp->minLog2Cols);

        if (tp->minLog2Tiles)
            maxTileAreaSb >>= (tp->minLog2Tiles + 1);
        tp->maxTileHeightSb = IPP_MAX(1, maxTileAreaSb / widestTileWidth);

        for (int32_t i = 0; i < tp->cols; i++) {
            // 8x8 units
            tp->miColStart[i] = tp->colStart[i] << 3;
            tp->miColEnd[i]   = IPP_MIN(tp->colEnd[i] << 3, miCols);
            tp->miColWidth[i] = tp->miColEnd[i] - tp->miColStart[i];

            for (int32_t j = tp->colStart[i]; j < tp->colEnd[i]; j++)
                tp->mapSb2TileCol[j] = i;
        }

        for (int32_t i = 0; i < tp->rows; i++) {
            // 8x8 units
            tp->miRowStart[i]  = tp->rowStart[i] << 3;
            tp->miRowEnd[i]    = IPP_MIN(tp->rowEnd[i] << 3, miRows);
            tp->miRowHeight[i] = tp->miRowEnd[i] - tp->miRowStart[i];

            for (int32_t j = tp->rowStart[i]; j < tp->rowEnd[i]; j++)
                tp->mapSb2TileRow[j] = i;
        }
    }

    void SetFrameDataAllocInfo(FrameData::AllocInfo &allocInfo, int32_t widthLu, int32_t heightLu, int32_t paddingLu, int32_t fourcc, void *m_fei, int32_t feiLumaPitch, int32_t feiChromaPitch)
    {
        int32_t bpp;
        int32_t heightCh;

        allocInfo.bitDepthLu = 8;
        allocInfo.bitDepthCh = 8;
        allocInfo.chromaFormat = 0/*MFX_CHROMAFORMAT_YUV420*/;
        allocInfo.paddingChH = paddingLu/2;
        heightCh = heightLu/2;
        bpp = 1;

        int32_t widthCh = widthLu;
        allocInfo.width = widthLu;
        allocInfo.height = heightLu;
        allocInfo.paddingLu = paddingLu;
        allocInfo.paddingChW = paddingLu;

        allocInfo.feiHdl = m_fei;
        if (m_fei) {
            assert(feiLumaPitch >= AlignValue( AlignValue(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 ));
            assert(feiChromaPitch >= AlignValue( AlignValue(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 ));
            assert((feiLumaPitch & 63) == 0);
            assert((feiChromaPitch & 63) == 0);
            allocInfo.alignment = 0x1000;
            allocInfo.pitchInBytesLu = feiLumaPitch;
            allocInfo.pitchInBytesCh = feiChromaPitch;
            allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
            allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        } else {
            allocInfo.alignment = 64;
            allocInfo.pitchInBytesLu = AlignValue( AlignValue(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 );
            allocInfo.pitchInBytesCh = AlignValue( AlignValue(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 );
            if ((allocInfo.pitchInBytesLu & (allocInfo.pitchInBytesLu - 1)) == 0)
                allocInfo.pitchInBytesLu += 64;
            if ((allocInfo.pitchInBytesCh & (allocInfo.pitchInBytesCh - 1)) == 0)
                allocInfo.pitchInBytesCh += 64;
            allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
            allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        }
    }
};  // namespace
