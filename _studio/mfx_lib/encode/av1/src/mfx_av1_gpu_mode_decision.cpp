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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "vector"
#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_gpu_mode_decision.h"

namespace AV1Enc {

void PrototypeGpuModeDecision(Frame *frame)
{
    assert(!frame->IsIntra());

    AV1VideoParam par = frame->m_fenc->m_videoParam;

    SetupTileParamAv1(&par, (par.sb64Rows + 0) / 1, (par.sb64Cols + 0) / 1, 0);

    std::vector<TileContexts> tctx(par.tileParam.rows * par.tileParam.cols);
    for (int32_t tr = 0, ti = 0; tr < par.tileParam.rows; tr++) {
        for (int32_t tc = 0; tc < par.tileParam.cols; tc++, ti++) {
            tctx[ti].Alloc(par.tileParam.colWidth[tc], par.tileParam.rowHeight[tr]);
            tctx[ti].Clear();
        }
    }
    frame->m_tileContexts.swap(tctx);

    const int32_t numCoeffPerSb = (1 << (LOG2_MAX_CU_SIZE << 1)) * 3 / 2; // assuming nv12
    AV1FrameEncoder *fenc = frame->m_fenc;

    AV1CU<uint8_t> *cu = (AV1CU<uint8_t> *)AV1_Malloc(sizeof(AV1CU<uint8_t>));
    ModeInfo *tempModeInfo = (ModeInfo *)AV1_Malloc(sizeof(ModeInfo) * 64 * 4); // 4 layers, 64 elements each

    Zero(*cu);

    for (int32_t row = 0, ctbAddr = 0; row < par.sb64Rows; row++) {
        for (int32_t col = 0; col < par.sb64Cols; col++, ctbAddr++) {
            CoeffsType *coefs = fenc->m_coeffWork + ctbAddr * numCoeffPerSb;
            ModeInfo *cuData = fenc->m_frame->m_modeInfo + ((col + row * par.miPitch) << 3);

            cu->InitCu_SwPath(par, cuData, tempModeInfo, row, col, *frame, coefs);
            cu->ModeDecisionInterTu7<CODEC_AV1, 0>(row << 3, col << 3);

            const int32_t ctbColWithinTile = col - (cu->m_tileBorders.colStart >> 3);
            const int32_t ctbRowWithinTile = row - (cu->m_tileBorders.rowStart >> 3);
            const TileContexts &tileCtx = fenc->m_frame->m_tileContexts[ GetTileIndex(par.tileParam, row, col) ];
            As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4)) = As16B(cu->m_contexts.aboveNonzero[0]);
            As8B(tileCtx.aboveNonzero[1]  + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.aboveNonzero[1]);
            As8B(tileCtx.aboveNonzero[2]  + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.aboveNonzero[2]);
            As16B(tileCtx.leftNonzero[0]  + (ctbRowWithinTile << 4)) = As16B(cu->m_contexts.leftNonzero[0]);
            As8B(tileCtx.leftNonzero[1]   + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftNonzero[1]);
            As8B(tileCtx.leftNonzero[2]   + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftNonzero[2]);
            As8B(tileCtx.abovePartition   + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.abovePartition);
            As8B(tileCtx.leftPartition    + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftPartition);
        }
    }

    for (int32_t row = par.sb64Rows - 1, ctbAddr = par.sb64Rows * par.sb64Cols - 1; row >= 0; row--) {
        for (int32_t col = par.sb64Cols - 1; col >= 0; col--, ctbAddr--) {
            CoeffsType *coefs = fenc->m_coeffWork + ctbAddr * numCoeffPerSb;
            ModeInfo *cuData = fenc->m_frame->m_modeInfo + ((col + row * par.miPitch) << 3);

            cu->InitCu_SwPath(par, cuData, tempModeInfo, row, col, *frame, coefs);
            cu->ModeDecisionInterTu7_SecondPass(row << 3, col << 3);
        }
    }

    frame->m_tileContexts.swap(tctx);
    AV1_Free(cu);
    AV1_Free(tempModeInfo);
}

};  // namespace AV1Enc

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
