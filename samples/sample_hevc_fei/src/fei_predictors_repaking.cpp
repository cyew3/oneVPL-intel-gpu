/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "fei_predictors_repacking.h"
#include <algorithm>

const mfxU8 ZigzagOrder[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

PredictorsRepaking::PredictorsRepaking() :
    m_max_fei_enc_mvp_num(4),
    m_repakingMode(PERFORMANCE),
    m_width(0),
    m_height(0),
    m_preencDSstrength(0),
    m_widthCU_ds(0),
    m_heightCU_ds(0),
    m_widthCU_enc(0),
    m_heightCU_enc(0)
{};

mfxStatus PredictorsRepaking::Init(mfxU8 mode, mfxU8 preencDSstrength, const mfxVideoParam& videoParams)
{
    if (videoParams.mfx.FrameInfo.Width == 0 || videoParams.mfx.FrameInfo.Height == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_repakingMode = mode;
    m_width = videoParams.mfx.FrameInfo.Width;
    m_height = videoParams.mfx.FrameInfo.Height;
    m_preencDSstrength = preencDSstrength;

    m_widthCU_ds   = (MSDK_ALIGN16((MSDK_ALIGN16(m_width) >> m_preencDSstrength))) >> 4;
    m_heightCU_ds  = (MSDK_ALIGN16((MSDK_ALIGN16(m_height) >> m_preencDSstrength))) >> 4;
    m_widthCU_enc  = (MSDK_ALIGN32(m_width)) >> 4;
    m_heightCU_enc = (MSDK_ALIGN32(m_height)) >> 4;

    return MFX_ERR_NONE;
};

mfxStatus PredictorsRepaking::RepackPredictors(const HevcTask& eTask, mfxExtFeiHevcEncMVPredictors& mvp)
{
    mfxStatus sts = MFX_ERR_NONE;

    switch (m_repakingMode)
    {
    case PERFORMANCE:
        sts = RepackPredictorsPerformance(eTask, mvp);
        break;
    case QUALITY:
        sts = RepackPredictorsQuality(eTask, mvp);
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return sts;
};

mfxStatus PredictorsRepaking::RepackPredictorsPerformance(const HevcTask& eTask, mfxExtFeiHevcEncMVPredictors& mvp)
{
    std::vector<mfxExtFeiPreEncMVExtended*> mvs_vec;
    std::vector<const RefIdxPair*>          refIdx_vec;

    mfxU8 numPredPairs = (std::min)(m_max_fei_enc_mvp_num, (std::max)(eTask.m_numRefActive[0], eTask.m_numRefActive[1]));
    if (numPredPairs != eTask.m_preEncOutput.size())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (numPredPairs == 0 || (eTask.m_frameType & MFX_FRAMETYPE_I))
        return MFX_ERR_NONE; // I-frame

    mvs_vec.reserve(m_max_fei_enc_mvp_num);
    refIdx_vec.reserve(m_max_fei_enc_mvp_num);

    // PreENC parameters reading
    for (std::list<PreENCOutput>::const_iterator it = eTask.m_preEncOutput.begin(); it != eTask.m_preEncOutput.end(); ++it)
    {
        mvs_vec.push_back((*it).m_mv);
        refIdx_vec.push_back(&(*it).m_refIdxPair);
    }

    const mfxI16Pair zeroPair = { 0, 0 };

    mfxU32 i = 0;
    // get nPred_actual L0/L1 predictors for each CU
    for (mfxU32 rowIdx = 0; rowIdx < m_heightCU_enc; ++rowIdx) // row index for full surface (raster-scan order)
    {
        for (mfxU32 colIdx = 0; colIdx < m_widthCU_enc; ++colIdx, ++i) // column index for full surface (raster-scan order)
        {
            // calculation of the input index for encoder after permutation from raster scan order index into 32x32 layout
            // HEVC encoder works with 32x32 layout
            mfxU32 permutEncIdx =
                ((colIdx >> 1) << 2)            // column offset;
                + (rowIdx & ~1) * m_widthCU_enc // offset for new line of 32x32 blocks layout;
                + (colIdx & 1)                  // zero or single offset depending on the number of comumn index;
                + ((rowIdx & 1) << 1);          // zero or double offset depending on the number of row index,
                                                // zero shift for top 16x16 blocks into 32x32 layout and double for bottom blocks;

            for (mfxU8 j = 0; j < numPredPairs; ++j)
            {
                mvp.Data[permutEncIdx].RefIdx[j].RefL0 = refIdx_vec[j]->RefL0;
                mvp.Data[permutEncIdx].RefIdx[j].RefL1 = refIdx_vec[j]->RefL1;

                if (m_preencDSstrength == 0)// w/o VPP
                {
                    if (colIdx >= m_widthCU_ds || rowIdx >= m_heightCU_ds)
                    {
                        mvp.Data[permutEncIdx].MV[j][0] = zeroPair;
                        mvp.Data[permutEncIdx].MV[j][1] = zeroPair;
                    }
                    else
                    {
                        mvp.Data[permutEncIdx].MV[j][0] = mvs_vec[j]->MB[i].MV[0][0];
                        mvp.Data[permutEncIdx].MV[j][1] = mvs_vec[j]->MB[i].MV[0][1];
                    }
                }
                else
                {
                    mfxU32 preencCUIdx; // index CU from PreENC output
                    mfxU32 rowMVIdx;    // row index for motion vector
                    mfxU32 colMVIdx;    // column index for motion vector
                    mfxU32 preencMVIdx; // linear index for motion vector

                    switch (m_preencDSstrength)
                    {
                    case 1:
                        preencCUIdx = (rowIdx >> 1) * m_widthCU_ds + (colIdx >> 1);
                        rowMVIdx = rowIdx & 1;
                        colMVIdx = colIdx & 1;
                        preencMVIdx = rowMVIdx * 8 + colMVIdx * 2;
                        break;
                    case 2:
                        preencCUIdx = (rowIdx >> 2) * m_widthCU_ds + (colIdx >> 2);
                        rowMVIdx = rowIdx & 3;
                        colMVIdx = colIdx & 3;
                        preencMVIdx = rowMVIdx * 4 + colMVIdx;
                        break;
                    case 3:
                        preencCUIdx = (rowIdx >> 3) * m_widthCU_ds + (colIdx >> 3);
                        rowMVIdx = rowIdx & 7;
                        colMVIdx = colIdx & 7;
                        preencMVIdx = rowMVIdx / 2 * 4 + colMVIdx / 2;
                        break;
                    default:
                        preencMVIdx = 0;
                        break;
                    }

                    mvp.Data[permutEncIdx].MV[j][0] = mvs_vec[j]->MB[preencCUIdx].MV[ZigzagOrder[preencMVIdx]][0];
                    mvp.Data[permutEncIdx].MV[j][1] = mvs_vec[j]->MB[preencCUIdx].MV[ZigzagOrder[preencMVIdx]][1];

                    mvp.Data[permutEncIdx].MV[j][0].x <<= m_preencDSstrength;
                    mvp.Data[permutEncIdx].MV[j][0].y <<= m_preencDSstrength;
                    mvp.Data[permutEncIdx].MV[j][1].x <<= m_preencDSstrength;
                    mvp.Data[permutEncIdx].MV[j][1].y <<= m_preencDSstrength;
                }
            }
        }
    }

    return MFX_ERR_NONE;
};

mfxStatus PredictorsRepaking::RepackPredictorsQuality(const HevcTask& eTask, mfxExtFeiHevcEncMVPredictors& mvp)
{
    return MFX_ERR_UNSUPPORTED;
};