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

#include <assert.h>
#include <numeric>
#include <algorithm>

#include "mfx_av1_encode.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"

using namespace AV1Enc;
using namespace MfxEnumShortAliases;

namespace AV1Enc {

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_cu_1[3][2][4][2] =
    {
        {
            {{0.0f, 0.0f},{6.068657e+000f, 1.955850e-001f},{2.794428e+001f, 1.643400e-001f},{4.697359e+002f, 1.113505e-001f},},
            {{0.0f, 0.0f},{8.335527e+000f, 2.312656e-001f},{1.282028e+001f, 2.317006e-001f},{3.218380e+001f, 2.157863e-001f},},
        },
        {
            {{0.0f, 0.0f},{1.412660e+001f, 1.845380e-001f},{1.967989e+001f, 1.873910e-001f},{3.617037e+002f, 1.257173e-001f},},
            {{0.0f, 0.0f},{2.319860e+001f, 2.194218e-001f},{2.436671e+001f, 2.260317e-001f},{3.785467e+001f, 2.252255e-001f},},
        },
        {
            {{0.0f, 0.0f},{1.465458e+001f, 1.946118e-001f},{1.725847e+001f, 1.969719e-001f},{3.298872e+002f, 1.336427e-001f},},
            {{0.0f, 0.0f},{3.133788e+001f, 2.192709e-001f},{3.292004e+001f, 2.266248e-001f},{3.572242e+001f, 2.357623e-001f},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_tu_1[3][2][4][2] =
    {
        {
            {{9.861009e+001f, 2.014005e-001f},{1.472963e+000f, 2.407408e-001f},{7.306935e+000f, 2.073082e-001f},{0.0f, 0.0f},},
            {{1.227516e+002f, 1.973595e-001f},{1.617983e+000f, 2.342001e-001f},{6.189427e+000f, 2.090359e-001f},{0.0f, 0.0f},},
        },
        {
            {{9.861009e+001f, 2.014005e-001f},{1.343445e+000f, 2.666583e-001f},{4.266056e+000f, 2.504071e-001f},{0.0f, 0.0f},},
            {{1.227516e+002f, 1.973595e-001f},{2.443985e+000f, 2.427829e-001f},{4.778531e+000f, 2.343920e-001f},{0.0f, 0.0f},},
        },
        {
            {{9.861009e+001f, 2.014005e-001f},{1.530746e+000f, 3.027675e-001f},{1.222733e+001f, 2.649870e-001f},{0.0f, 0.0f},},
            {{1.227516e+002f, 1.973595e-001f},{2.158767e+000f, 2.696284e-001f},{3.401860e+000f, 2.652995e-001f},{0.0f, 0.0f},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_cu_4[3][2][4][2] =
    {
        {
            {{0, 0},{4.218807e+000f, 2.066010e-001f},{2.560569e+001f, 1.677201e-001f},{4.697359e+002f, 1.113505e-001f},},
            {{0, 0},{4.848787e+000f, 2.299086e-001f},{1.198510e+001f, 2.170575e-001f},{3.218380e+001f, 2.157863e-001f},},
        },
        {
            {{0, 0},{5.227898e+000f, 2.117919e-001f},{1.819586e+001f, 1.910210e-001f},{3.617037e+002f, 1.257173e-001f},},
            {{0, 0},{1.098668e+001f, 2.226733e-001f},{2.273784e+001f, 2.157968e-001f},{3.785467e+001f, 2.252255e-001f},},
        },
        {
            {{0, 0},{4.591922e+000f, 2.238883e-001f},{1.480393e+001f, 2.032860e-001f},{3.298872e+002f, 1.336427e-001f},},
            {{0, 0},{1.831528e+001f, 2.201327e-001f},{2.999500e+001f, 2.175826e-001f},{3.572242e+001f, 2.357623e-001f},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_tu_4[3][2][4][2] =
    {
        {
            {{1.008684e+002f, 2.003675e-001f},{2.278697e+000f, 2.226109e-001f},{1.767972e+001f, 1.733893e-001f},{0.0f, 0.0f},},
            {{1.281323e+002f, 1.984014e-001f},{2.906725e+000f, 2.063635e-001f},{1.106598e+001f, 1.798757e-001f},{0.0f, 0.0f},},
        },
        {
            {{1.008684e+002f, 2.003675e-001f},{2.259367e+000f, 2.473857e-001f},{3.871648e+001f, 1.789534e-001f},{0.0f, 0.0f},},
            {{1.281323e+002f, 1.984014e-001f},{4.119740e+000f, 2.092187e-001f},{9.931154e+000f, 1.907941e-001f},{0.0f, 0.0f},},
        },
        {
            {{1.008684e+002f, 2.003675e-001f},{1.161667e+000f, 3.112699e-001f},{5.179975e+001f, 2.364036e-001f},{0.0f, 0.0f},},
            {{1.281323e+002f, 1.984014e-001f},{4.326205e+000f, 2.262409e-001f},{2.088369e+001f, 1.829562e-001f},{0.0f, 0.0f},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_cu_7[3][2][4][2] =
    {
        {
            {{0.0f, 0.0f},{0.0f, 0.0f},{2.868749e+001f, 1.759965e-001f},{0.0f, 0.0f},},
            {{0.0f, 0.0f},{0.0f, 0.0f},{1.203077e+001f, 2.066983e-001f},{0.0f, 0.0f},},
        },
        {
            {{0.0f, 0.0f},{0.0f, 0.0f},{2.689497e+001f, 1.914970e-001f},{0.0f, 0.0f},},
            {{0.0f, 0.0f},{0.0f, 0.0f},{1.856055e+001f, 2.129173e-001f},{0.0f, 0.0f},},
        },
            {{{0.0f, 0.0f},{0.0f, 0.0f},{2.268480e+001f, 2.049253e-001f},{0.0f, 0.0f},},
            {{0.0f, 0.0f},{0.0f, 0.0f},{3.414455e+001f, 2.067140e-001f},{0.0f, 0.0f},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    float h265_split_thresholds_tu_7[3][2][4][2] = {{
        {{7.228049e+001f, 1.900956e-001f},{2.365207e+000f, 2.211628e-001f},{4.651502e+001f, 1.425675e-001f},{0.0f, 0.0f},},
        {{0.0f, 0.0f},{2.456053e+000f, 2.100343e-001f},{7.537189e+000f, 1.888014e-001f},{0.0f, 0.0f},},},
    {{{7.228049e+001f, 1.900956e-001f},{1.293689e+000f, 2.672417e-001f},{1.633606e+001f, 2.076887e-001f},{0.0f, 0.0f},},
    {{0.0f, 0.0f},{1.682124e+000f, 2.368017e-001f},{5.516403e+000f, 2.080632e-001f},{0.0f, 0.0f},},
    },{{{7.228049e+001f, 1.900956e-001f},{1.479032e+000f, 2.785342e-001f},{8.723769e+000f, 2.469559e-001f},{0.0f, 0.0f},},
    {{0.0f, 0.0f},{1.943537e+000f, 2.548912e-001f},{1.756701e+001f, 1.900659e-001f},{0.0f, 0.0f},},
    },};


    typedef float thres_tab[2][4][2];

    //static
    CostType h265_calc_split_threshold(int32_t tabIndex, int32_t isNotCu, int32_t isNotI, int32_t log2width, int32_t strength, int32_t QP)
    {
        thres_tab *h265_split_thresholds_tab;

        switch(tabIndex) {
        case 1:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_1 : h265_split_thresholds_cu_1;
            break;
        default:
        case 0:
        case 2:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_4 : h265_split_thresholds_cu_4;
            break;
        case 3:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_7 : h265_split_thresholds_cu_7;
            break;
        }

        if (strength == 0)
            return 0;
        if (strength > 3)
            strength = 3;

        if (isNotCu) {
            if ((log2width < 3 || log2width > 5)) {
                return 0;
            }
        }
        else {
            if ((log2width < 4 || log2width > 6)) {
                return 0;
            }
        }

        CostType a = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][0];
        CostType b = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][1];
        return a * exp(b * QP);
    }
};


const float tab_rdLambdaBPyramid5LongGop[5]  = {0.442f, 0.3536f, 0.3536f, 0.4000f, 0.68f};
const float tab_rdLambdaBPyramid5[5]         = {0.442f, 0.3536f, 0.3536f, 0.3536f, 0.68f};
const float tab_rdLambdaBPyramid4[4]         = {0.442f, 0.3536f, 0.3536f, 0.68f};
const float tab_rdLambdaBPyramid_LoCmplx[4]  = {0.442f, 0.3536f, 0.4000f, 0.68f};
const float tab_rdLambdaBPyramid_MidCmplx[4] = {0.442f, 0.3536f, 0.3817f, 0.60f};
//const float tab_rdLambdaBPyramid_HiCmplx[4]  = {0.442f, 0.2793f, 0.3536f, 0.50f}; //
const float tab_rdLambdaBPyramid_HiCmplx[4]  = {0.442f, 0.3536f, 0.3536f, 0.50f};


namespace AV1Enc {

    bool SliceLambdaMultiplier(CostType &rd_lambda_slice, AV1VideoParam const & videoParam, uint8_t frameType, const Frame *currFrame, bool isHiCmplxGop, bool isMidCmplxGop)
    {
        bool mult = false;
        switch (frameType) {
        case MFX_FRAMETYPE_P:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                rd_lambda_slice = (currFrame->m_biFramesInMiniGop == 15)
                    ? (videoParam.longGop)
                    ? tab_rdLambdaBPyramid5LongGop[0]
                : tab_rdLambdaBPyramid5[0]
                : tab_rdLambdaBPyramid4[0];
                if (videoParam.DeltaQpMode&AMT_DQP_CAQ) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            else {
                int32_t pgopIndex = currFrame->m_pyramidLayer; //(currFrame->m_frameOrder - currFrame->m_frameOrderOfLastIntra) % videoParam.PGopPicSize;
                rd_lambda_slice = (videoParam.PGopPicSize == 1 || pgopIndex) ? 0.4624f : 0.578f;
                mult = !!pgopIndex;
                if (!pgopIndex && (videoParam.DeltaQpMode&AMT_DQP_CAQ)) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            break;
        case MFX_FRAMETYPE_B:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                int layer = currFrame->m_pyramidLayer;
                if (videoParam.DeltaQpMode&AMT_DQP_CAL) {
                    if (isHiCmplxGop)
                        rd_lambda_slice = tab_rdLambdaBPyramid_HiCmplx[layer];
                    else if (isMidCmplxGop)
                        rd_lambda_slice = tab_rdLambdaBPyramid_MidCmplx[layer];
                    else
                        rd_lambda_slice = tab_rdLambdaBPyramid_LoCmplx[layer];
                }
                else
                {
                    rd_lambda_slice = (currFrame->m_biFramesInMiniGop == 15)
                        ? (videoParam.longGop)
                            ? tab_rdLambdaBPyramid5LongGop[layer]
                            : tab_rdLambdaBPyramid5[layer]
                        : tab_rdLambdaBPyramid4[layer];
                }
                mult = !!layer;
                if (!layer && (videoParam.DeltaQpMode&AMT_DQP_CAQ)) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            else {
                rd_lambda_slice = 0.4624f;
                mult = true;
            }
            break;
        case MFX_FRAMETYPE_I:
        default:
            rd_lambda_slice = 0.57f;
            if (videoParam.GopRefDist > 1)
                rd_lambda_slice *= (1 - MIN(0.5f, 0.05f * (videoParam.GopRefDist - 1)));
            if (videoParam.DeltaQpMode&AMT_DQP_CAQ) {
                rd_lambda_slice*=videoParam.LambdaCorrection;
            }
        }
        return mult;
    }
}

static uint8_t dpoc_neg[][16][6] = {
    //GopRefDist 4
    {{2, 4, 2},
    {1, 1},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{4, 8, 2, 2, 4}, //poc 8
    {1, 1},           //poc 1
    {2, 2, 2},
    {2, 1, 2},
    {3, 4, 2, 2},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4}},

    //GopRefDist 16.
    // it isn't BPyramid. it is "anchor & chain"
    {{5, 16, 2, 2, 4, 8}, //poc 16
    {2, 1, 16},           // poc 1
    {3, 2, 4, 12},
    {3, 1, 2, 16},
    {4, 4, 4, 4, 8},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4},
    {4, 4, 2, 2, 16},
    {2, 1, 8},
    {3, 2, 2, 6},
    {3, 1, 2, 8},
    {4, 4, 2, 2, 4},
    {3, 1, 4, 8},
    {4, 2, 2, 2, 8},
    {4, 1, 2, 4, 8}}  // poc 15
};

static uint8_t dpoc_pos[][16][6] = {
    //GopRefDist 4
    {{0},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{0,},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 16
    {{0,},        // poc 16
    {3, 1, 2, 12},// poc 1
    {2, 2, 12},
    {2, 1, 12},
    {1, 12},
    {3, 1, 2, 8},
    {2, 2, 8},
    {2, 1, 8},
    {1, 8},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}} // poc 15
};


AV1FrameEncoder::AV1FrameEncoder(AV1Encoder& topEnc)
    : m_topEnc(topEnc)
    , m_videoParam(topEnc.m_videoParam)
    , m_dataTemp(topEnc.data_temp)
    , m_coeffWork(nullptr)
    , m_Palette8x8(nullptr)
    , m_ColorMapY(nullptr)
    , m_ColorMapUV(nullptr)
#if ENABLE_TPLMV
    , m_tplMvs(nullptr)
#endif
    , m_txkTypes4x4(nullptr)
    , m_cdefStrenths(nullptr)
    , m_cfl(nullptr)
    , m_workBufForHash(nullptr)
    , m_frame(nullptr)
{
}

void AV1FrameEncoder::Init()
{
    m_dataTempSize = 4 << m_videoParam.Log2NumPartInCU;

    const int32_t bitPerPixel = m_videoParam.bitDepthLuma + (8 * m_videoParam.bitDepthChroma >> m_videoParam.chromaShift) / 4;
    const int32_t frameSize = m_videoParam.Width * m_videoParam.Height * bitPerPixel / 8;
#if ENABLE_PRECARRY_BUF
    m_totalBitstreamBuffer.resize(frameSize * 3); // uint8_t bitstream buffer + uint16_t temp buffer
#else

#if ENABLE_BITSTREAM_MEM_REDUCTION
    if (!m_videoParam.ZeroDelay || m_topEnc.m_brc) {
        m_totalBitstreamBuffer.resize(frameSize);
    }
#else
    m_totalBitstreamBuffer.resize(frameSize);
#endif

#endif

    const int32_t maxNumTiles = std::max(m_videoParam.tileParamKeyFrame.numTiles, m_videoParam.tileParamInterFrame.numTiles);
    m_tilePtrs.resize(maxNumTiles);
    m_tileSizes.resize(maxNumTiles);

    if (!PACK_BY_TILES)
        m_tileBc.resize(maxNumTiles);

    m_tileContexts.resize(maxNumTiles);
    for (auto &tc: m_tileContexts)
        tc.Alloc(m_videoParam.sb64Cols, m_videoParam.sb64Rows); // allocate for entire frame to be ready to switch to single tile encoding

    m_entropyContexts.resize(maxNumTiles);

    const uint32_t numCtbs = m_videoParam.PicWidthInCtbs * m_videoParam.PicHeightInCtbs;
    const int32_t numCoefs = (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift);
    const uint32_t coeffWorkSize = sizeof(CoeffsType) * numCoefs;
    m_coeffWork = (CoeffsType *)AV1_Malloc(coeffWorkSize);
    ThrowIf(!m_coeffWork, std::bad_alloc());

    m_Palette8x8 = (PaletteInfo*)AV1_Malloc(m_videoParam.miRows * m_videoParam.miPitch * sizeof(PaletteInfo));
    if (!m_Palette8x8)
        throw std::exception();

    m_ColorMapYPitch = ((m_videoParam.Width + 63)&~63);
    m_ColorMapY = (uint8_t*)AV1_Malloc(m_ColorMapYPitch * m_videoParam.Height);
    if (!m_ColorMapY)
        throw std::exception();

    m_ColorMapUVPitch = ((((m_videoParam.Width >> m_videoParam.subsamplingX) << 1) + 63)&~63);
    m_ColorMapUV = (uint8_t*)AV1_Malloc(m_ColorMapUVPitch * (m_videoParam.Height >> m_videoParam.subsamplingY));
    if (!m_ColorMapUV)
        throw std::exception();
#if ENABLE_TPLMV
    m_tplMvs = (TplMvRef *)AV1_Malloc((m_videoParam.miRows + 8) * m_videoParam.miPitch * sizeof(*m_tplMvs));
    if (!m_tplMvs)
        throw std::exception();
#endif

    m_txkTypes4x4 = (uint16_t *)AV1_Malloc(m_videoParam.sb64Rows * m_videoParam.sb64Cols * sizeof(uint16_t) * 256);
    if (!m_txkTypes4x4)
        throw std::exception();

    m_cdefStrenths = (int8_t *)AV1_Malloc(m_videoParam.sb64Rows * m_videoParam.sb64Cols * sizeof(*m_cdefStrenths)); // ippMalloc is 64-bytes aligned
    if (!m_cdefStrenths)
        throw std::exception();

    if (m_videoParam.src10Enable)
        m_cdefLineBufs10.Alloc(m_videoParam);
    else
        m_cdefLineBufs.Alloc(m_videoParam);

    if (m_videoParam.cflFlag) {
        m_cfl = (CFL_params *)AV1_Malloc(m_videoParam.sb64Rows * m_videoParam.sb64Cols * sizeof(CFL_params) * 64); // for 4x4 subblocks
        if (!m_cfl)
            throw std::exception();
    }
}


void AV1FrameEncoder::Close()
{
    AV1_SafeFree(m_coeffWork);
    AV1_SafeFree(m_Palette8x8);
    AV1_SafeFree(m_ColorMapY);
    AV1_SafeFree(m_ColorMapUV);
#if ENABLE_TPLMV
    AV1_SafeFree(m_tplMvs);
#endif
    AV1_SafeFree(m_txkTypes4x4);
    AV1_SafeFree(m_cdefStrenths);
    AV1_SafeFree(m_cfl);
    if (m_videoParam.src10Enable)
        m_cdefLineBufs10.Free();
    else
        m_cdefLineBufs.Free();


    for (auto &p: m_tmpBufForHash) {
        if (p) {
            AV1_Free(p);
            p = nullptr;
        }
    }
#if ENABLE_HASH_MEM_REDUCTION
    AV1_SafeFree(m_workBufForHash);
#endif
}

//TO BE DONE
// in display order
int32_t TAB_BPYRAMID_LAYER[16][15] = {
    {1},
    {1},
    {2,1},//2
    {2,1,2},//3
    {3,2,1,2},//4
    {3,2,1,3,2},//5

    {3,2,3,1,3,2},//6
    {3,2,3,1,3,2,3},//7

    {4,3,2,3,1,3,2,3},//8

    {4,3,2,3,1,4,3,2,3},//9
    {4,3,2,4,3,1,4,3,2,3},//10

    {4,3,2,3,1,4,3,2,3},//11
    {4,3,2,4,3,1,4,3,4,2,4,3},//12

    {4,3,2,4,3,4,1,4,3,4,2,4,3},//13
    {4,3,4,2,4,3,4,1,4,3,4,2,4,3},//14
    {4,3,4,2,4,3,4,1,4,3,4,2,4,3,4}//15
};

struct isEqual
{
    isEqual(int32_t frameOrder): m_frameOrder(frameOrder)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_frameOrder == m_frameOrder;
    }

    int32_t m_frameOrder;
};

// pyramidLayer is not quite temporal id, so need this conversion
static int32_t GetTemporalId(int32_t pyramidLayer, int32_t numTemporalLayers, int32_t gopRefDist)
{
    if (gopRefDist > 1)
        return 0; // for now no B-pyramid temporal layers

    assert(pyramidLayer < 3);
    assert(numTemporalLayers >= 1 && numTemporalLayers <= 3);
    if (numTemporalLayers == 1)
        return 0;
    if (numTemporalLayers == 3)
        return pyramidLayer;
    return pyramidLayer >> 1;
}

void AV1Encoder::ConfigureInputFrame(Frame* frame, bool bEncOrder, bool bEncCtrl) const
{
    if (!bEncOrder)
        frame->m_frameOrder = m_frameOrder;

    frame->curFrameOffset = frame->m_frameOrder & ((1 << m_videoParam.seqParams.orderHintBits) - 1);
    frame->m_frameOrderOfLastIdr = m_frameOrderOfLastIdr;
    frame->m_frameOrderOfLastIntra = m_frameOrderOfLastIntra;
    frame->m_frameOrderOfLastBaseTemporalLayer = m_frameOrderOfLastBaseTemporalLayer;
    frame->m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchor;
    //frame->m_frameOrderOfLastExternalLTR = (m_frameOrderOfLastExternalLTR[0] == m_frameOrder) ? m_frameOrderOfLastExternalLTR[1] : m_frameOrderOfLastExternalLTR[0];

    if (bEncOrder || !bEncCtrl) {
        frame->m_poc = frame->m_frameOrder - frame->m_frameOrderOfLastIdr;
        frame->m_isIdrPic = false;
    }

    if (!bEncOrder && !bEncCtrl) {
        int32_t idrDist = m_videoParam.GopPicSize * m_videoParam.IdrInterval;
        if (m_videoParam.picStruct == PROGR) {
            if (frame->m_frameOrder == 0 || idrDist > 0 && (frame->m_frameOrder - frame->m_frameOrderOfLastIdr) % idrDist == 0) {
                frame->m_isIdrPic = true;
                frame->m_picCodeType = MFX_FRAMETYPE_I;
                frame->m_poc = 0;
            } else if (m_videoParam.GopPicSize > 0 && (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.GopPicSize == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_I;
            } else if ((frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_P;
            } else {
                frame->m_picCodeType = MFX_FRAMETYPE_B;
            }
        } else {
            if (frame->m_frameOrder/2 == 0 || idrDist > 0 && (frame->m_frameOrder/2 - frame->m_frameOrderOfLastIdr/2) % idrDist == 0) {
                if (frame->m_frameOrder & 1) {
                    frame->m_picCodeType = MFX_FRAMETYPE_P;
                } else {
                    frame->m_isIdrPic = true;
                    frame->m_picCodeType = MFX_FRAMETYPE_I;
                    frame->m_poc = 0;
                }
            } else if (m_videoParam.GopPicSize > 0 && (frame->m_frameOrder/2 - frame->m_frameOrderOfLastIntra/2) % m_videoParam.GopPicSize == 0) {
                if (frame->m_frameOrder & 1)
                    frame->m_picCodeType = MFX_FRAMETYPE_I;
                else
                    frame->m_picCodeType = MFX_FRAMETYPE_P;
            } else if ((frame->m_frameOrder/2 - frame->m_frameOrderOfLastAnchor/2) % m_videoParam.GopRefDist == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_P;
            } else {
                frame->m_picCodeType = MFX_FRAMETYPE_B;
            }
        }
    } else if (bEncOrder) {
        if (frame->m_picCodeType & MFX_FRAMETYPE_IDR) {
            frame->m_picCodeType = MFX_FRAMETYPE_I;
            frame->m_isIdrPic = true;
            frame->m_poc = 0;
            frame->m_isRef = 1;
        } else if (frame->m_picCodeType & MFX_FRAMETYPE_REF) {
            frame->m_picCodeType &= ~MFX_FRAMETYPE_REF;
            frame->m_isRef = 1;
        }
    }

    frame->intraOnly = !!(frame->m_picCodeType & MFX_FRAMETYPE_I);

    if (!bEncOrder) {
        double frameDuration = (double)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000;
        if (frame->m_timeStamp == MFX_TIMESTAMP_UNKNOWN)
            frame->m_timeStamp = (m_lastTimeStamp == MFX_TIMESTAMP_UNKNOWN) ? 0 : int64_t(m_lastTimeStamp + frameDuration);

        frame->m_RPSIndex = (m_videoParam.GopRefDist > 1)
            ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist
            : (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.PGopPicSize;
        frame->m_miniGopCount = m_miniGopCount + !(frame->m_picCodeType == MFX_FRAMETYPE_B);

        if (m_videoParam.PGopPicSize > 1) {
            frame->m_pyramidLayer = h265_pgop_layers[frame->m_RPSIndex];
            frame->m_temporalId = GetTemporalId(frame->m_pyramidLayer, m_videoParam.NumRefLayers, m_videoParam.GopRefDist);
        }
    } else {
        double frameDuration = (double)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000;
        if (frame->m_timeStamp == MFX_TIMESTAMP_UNKNOWN)
            frame->m_timeStamp = (m_lastTimeStamp == MFX_TIMESTAMP_UNKNOWN) ? 0 : int64_t(m_lastTimeStamp + frameDuration * (frame->m_frameOrder - m_frameOrder));

        if (frame->m_picCodeType != MFX_FRAMETYPE_B) {
            frame->m_miniGopCount = m_miniGopCount - (frame->m_picCodeType == MFX_FRAMETYPE_B);
        } else {
            int32_t miniGopCountOfAnchor = m_miniGopCount;
            FrameList* queue[3] = {(FrameList*)&m_inputQueue, (FrameList*)&m_reorderedQueue, (FrameList*)&m_dpb};
            for (int32_t idx = 0; idx < 3; idx++) {
                FrameIter it = std::find_if(queue[idx]->begin(), queue[idx]->end(), isEqual(frame->m_frameOrderOfLastAnchor));
                if (it != queue[idx]->end()) {
                    miniGopCountOfAnchor = (*it)->m_miniGopCount;
                    break;
                }
            }
            frame->m_miniGopCount = miniGopCountOfAnchor + !(frame->m_picCodeType == MFX_FRAMETYPE_B);
        }

        frame->m_biFramesInMiniGop = m_LastbiFramesInMiniGop;

        //frame->m_RPSIndex = (frame->m_picCodeType == MFX_FRAMETYPE_B) ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) : 0;
        frame->m_RPSIndex = (m_videoParam.GopRefDist > 1)
            ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist
            : (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.PGopPicSize;
        if (m_videoParam.PGopPicSize > 1) {
            frame->m_pyramidLayer = h265_pgop_layers[frame->m_RPSIndex];
            frame->m_temporalId = GetTemporalId(frame->m_pyramidLayer, m_videoParam.NumRefLayers, m_videoParam.GopRefDist);
        }

        int32_t biPyramid = m_videoParam.BiPyramidLayers > 1;
        int32_t posB = (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist;
        if (posB && frame->m_picCodeType == MFX_FRAMETYPE_B && biPyramid) {
            frame->m_pyramidLayer = TAB_BPYRAMID_LAYER[m_videoParam.GopRefDist-1][posB-1];
        }
    }
}


void AV1Encoder::UpdateGopCounters(Frame *inputFrame, bool bEncOrder)
{
    if (!bEncOrder) {
        m_frameOrder++;
        m_lastTimeStamp = inputFrame->m_timeStamp;

        if (inputFrame->m_isIdrPic)
            m_frameOrderOfLastIdr = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType == MFX_FRAMETYPE_I)
            m_frameOrderOfLastIntra = inputFrame->m_frameOrder;
        if (inputFrame->m_temporalId == 0)
            m_frameOrderOfLastBaseTemporalLayer = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType != MFX_FRAMETYPE_B) {
            m_frameOrderOfLastAnchor = inputFrame->m_frameOrder;
            m_miniGopCount++;
        }
    }
    else {
        m_frameOrder    = inputFrame->m_frameOrder;
        m_lastTimeStamp = inputFrame->m_timeStamp;

        if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
            if (inputFrame->m_isIdrPic)
                m_frameOrderOfLastIdrB = inputFrame->m_frameOrder;
            if (inputFrame->m_picCodeType & MFX_FRAMETYPE_I)
                m_frameOrderOfLastIntraB = inputFrame->m_frameOrder;

            m_frameOrderOfLastAnchorB = inputFrame->m_frameOrder;
            m_LastbiFramesInMiniGop = m_frameOrderOfLastAnchorB  -  m_frameOrderOfLastAnchor - 1;
            assert(m_LastbiFramesInMiniGop < m_videoParam.GopRefDist);
            m_miniGopCount++;
        }
    }

}


void AV1Encoder::RestoreGopCountersFromFrame(Frame *frame, bool bEncOrder)
{
    assert (!bEncOrder);
    // restore global state
    m_frameOrder = frame->m_frameOrder;
    m_frameOrderOfLastIdr = frame->m_frameOrderOfLastIdr;
    m_frameOrderOfLastIntra = frame->m_frameOrderOfLastIntra;
    m_frameOrderOfLastBaseTemporalLayer = frame->m_frameOrderOfLastBaseTemporalLayer;
    m_frameOrderOfLastAnchor = frame->m_frameOrderOfLastAnchor;
    m_miniGopCount = frame->m_miniGopCount - !(frame->m_picCodeType == MFX_FRAMETYPE_B);
}

namespace {
    bool PocIsLess(const Frame *f1, const Frame *f2) { return f1->m_poc < f2->m_poc; }
    bool PocIsGreater(const Frame *f1, const Frame *f2) { return f1->m_poc > f2->m_poc; }

    void ModifyRefList(const AV1Enc::AV1VideoParam &par, Frame &currFrame, int32_t listIdx)
    {
        RefPicList &list = currFrame.m_refPicList[listIdx];
        Frame **refs = list.m_refFrames;
        int8_t *mod = list.m_listMod;
        int8_t *pocs = list.m_deltaPoc;
        uint8_t *lterms = list.m_isLongTermRef;

        list.m_listModFlag = 0;
        for (int i = 0; i < list.m_refFramesCount; i++)
            mod[i] = int8_t(i);

        if (currFrame.m_hasRefCtrl && par.GopRefDist == 1 && list.m_refFramesCount > 0) {
            int k = 0;
            int i = 0;
            for (i = 0; i < 32 && currFrame.refctrl.PreferredRefList[i].FrameOrder != 0xffffffff; i++) {
                int j = k;
                for (; j < list.m_refFramesCount; j++)
                    if ((int)currFrame.refctrl.PreferredRefList[i].FrameOrder == refs[j]->m_frameOrder) break;

                /*if (j >= list.m_refFramesCount) {
                    // not found
                    if (currFrame.refctrl.PreferredRefList[i].FrameOrder == (uint32_t)currFrame.m_frameOrderOfLastExternalLTR && par.EnableALTR) {
                        //LTR may be updated
                        Frame *ltrFrame = NULL;
                        int32_t ltridx = 1;
                        for (int32_t n = 0; n < list.m_refFramesCount; n++)
                        {
                            if (refs[n]->m_isLtrFrame) {
                                if (!ltrFrame) {
                                    ltrFrame = refs[n];
                                    ltridx = n;
                                }
                                else if (refs[n]->m_frameOrder > ltrFrame->m_frameOrder) {
                                    ltrFrame = refs[n];
                                    ltridx = n;
                                }
                            }
                        }
                        if (ltrFrame) {
                            j = ltridx;
                        }
                    }
                }*/
                if (j > k && j < list.m_refFramesCount) {
                    std::rotate(refs + k, refs + j, refs + j + 1);
                    std::rotate(mod + k, mod + j, mod + j + 1);
                    std::rotate(pocs + k, pocs + j, pocs + j + 1);
                    std::rotate(lterms + k, lterms + j, lterms + j + 1);
                    list.m_listModFlag = 1;
                    k++;
                }
            }

            /*k = list.m_refFramesCount;
            for (int i = 0; i < 16 && currFrame.refctrl.RejectedRefList[i].FrameOrder != 0xffffffff; i++) {
                int j;
                for (j = 0; j < k; j++)
                    if (currFrame.refctrl.RejectedRefList[i].FrameOrder == refs[j]->m_frameOrder) break;

                if (j < k - 1) {
                    std::rotate(refs + j, refs + j + 1, refs + k);
                    std::rotate(mod + j, mod + j + 1, mod + k);
                    std::rotate(pocs + j, pocs + j + 1, pocs + k);
                    std::rotate(lterms + j, lterms + j + 1, lterms + k);
                    list.m_listModFlag = 1;
                    k--;
                }
            }*/
            if (listIdx) {
                if (currFrame.refctrl.NumRefIdxL1Active)
                    list.m_refFramesCount = MIN(list.m_refFramesCount, currFrame.refctrl.NumRefIdxL1Active);
            } else {
                if (currFrame.refctrl.NumRefIdxL0Active) {
                    list.m_refFramesCount = MIN(list.m_refFramesCount, currFrame.refctrl.NumRefIdxL0Active);
                    if (currFrame.refctrl.NumRefIdxL0Active == 1 && refs[0]->m_isLtrFrame) {
                        // Infer Error Resilience Point
                        if (par.PGopPicSize > 1) {
                            if (par.NumRefLayers == 1 || !par.GopStrictFlag) {
                                currFrame.m_isErrorResilienceFrame = 1;
                                currFrame.m_isRef = 1;
                                currFrame.m_pyramidLayer = 0;
                                currFrame.m_temporalId = 0;
                            }
                            else if (par.NumRefLayers == 2 && currFrame.m_pyramidLayer < 2) {
                                currFrame.m_isErrorResilienceFrame = 1;
                            }
                            else if (par.NumRefLayers == 3 && currFrame.m_pyramidLayer < 1) {
                                currFrame.m_isErrorResilienceFrame = 1;
                            }
                        } else {
                            currFrame.m_isErrorResilienceFrame = 1;
                        }
                    }
                }
            }
        }
        else if (par.AdaptiveRefs) {
            // stable sort by pyramidLayer
#if defined(PARALLEL_LOW_DELAY)
            if (par.PGopPicSize > 1) {
                const int32_t refLayerLimit[3][3] =
                {
                    { 2, 2, 2 },  // 1 layer
                    { 1, 1, 1 },  // 2 layer
                    { 0, 1, 1 }   // 3 layer
                };
                // P pyramid with independence
                int32_t parallelLowDelay = MAX(0, MIN(2, (int) par.NumRefLayers -1));

                // limit refs
                for (int32_t i = 0; i < list.m_refFramesCount; i++)
                {
                    int32_t layerLimit = refLayerLimit[parallelLowDelay][MIN(currFrame.m_pyramidLayer, 2)];
                    int32_t j = i;
                    while (j < list.m_refFramesCount && refs[j]->m_pyramidLayer > layerLimit) j++;
                    if (j > i && j < list.m_refFramesCount) {
                        std::rotate(refs + i, refs + j, refs + j + 1);
                        std::rotate(mod + i, mod + j, mod + j + 1);
                        std::rotate(pocs + i, pocs + j, pocs + j + 1);
                        std::rotate(lterms + i, lterms + j, lterms + j + 1);
                        list.m_listModFlag = 1;
                    } else if (i && j > i && j >= list.m_refFramesCount) {
                        // not found
                        list.m_refFramesCount = i;
                        break;
                    }
                }
                if (list.m_refFramesCount>1 && refs[0]->m_pyramidLayer)
                {
                    // get best other in sorted order
                    for (int32_t i = 2; i < list.m_refFramesCount; i++) {
                        int32_t layerI = refs[i]->m_pyramidLayer;
                        int32_t j = i;
                        while (j > 1 && refs[j - 1]->m_pyramidLayer > layerI) j--;
                        if (j < i) {
                            std::rotate(refs + j, refs + i, refs + i + 1);
                            std::rotate(mod + j, mod + i, mod + i + 1);
                            std::rotate(pocs + j, pocs + i, pocs + i + 1);
                            std::rotate(lterms + j, lterms + i, lterms + i + 1);
                            list.m_listModFlag = 1;
                        }
                    }
                }
            }
            else
#endif
            {
                for (int32_t i = 2; i < list.m_refFramesCount; i++) {
                    int32_t layerI = refs[i]->m_pyramidLayer;
                    int32_t j = i;
                    while (j > 1 && refs[j - 1]->m_pyramidLayer > layerI) j--;
                    if (j < i) {
                        std::rotate(refs + j, refs + i, refs + i + 1);
                        std::rotate(mod + j, mod + i, mod + i + 1);
                        std::rotate(pocs + j, pocs + i, pocs + i + 1);
                        std::rotate(lterms + j, lterms + i, lterms + i + 1);
                        list.m_listModFlag = 1;
                    }
                }
            }

            if (par.picStruct == PROGR) {
                // cut ref lists for non-ref frames
                if (!currFrame.m_isRef && list.m_refFramesCount > 1) {
#ifdef PARALLEL_LOW_DELAY_FAST
                    if (currFrame.m_picCodeType == MFX_FRAMETYPE_P) {
                        // fast non ref P when parallel low delay
                        list.m_refFramesCount = 1;
                    } else
#else
                    if (currFrame.m_picCodeType == MFX_FRAMETYPE_B)
#endif
                    {
                        int32_t layer0 = refs[0]->m_pyramidLayer;
                        for (int32_t i = 1; i < list.m_refFramesCount; i++) {
                            if (refs[i]->m_pyramidLayer >= layer0) {
                                list.m_refFramesCount = i;
                                break;
                            }
                        }
                    }
                }
                // cut ref lists for B pyramid
                if (par.BiPyramidLayers == 4) {
                    // list is already sorted by m_pyramidLayer (ascending)
                    int32_t refLayerLimit = par.refLayerLimit[currFrame.m_pyramidLayer];
                    for (int32_t i = 1; i < list.m_refFramesCount; i++) {
                        if (refs[i]->m_pyramidLayer > refLayerLimit) {
                            list.m_refFramesCount = i;
                            break;
                        }
                    }
                }
            }

            // LTR RPL Mod for Efficiency. L0[1] = ltr
            if (listIdx == 0 && list.m_refFramesCount > 2 && !currFrame.m_shortTermRefFlag) {
                Frame *ltrFrame = NULL;
                int32_t ltridx = 1;
                for (int32_t i = 0; i < list.m_refFramesCount; i++)
                {
                    if (refs[i]->m_isLtrFrame) {
                        if (!ltrFrame) {
                            ltrFrame = refs[i];
                            ltridx = i;
                        }
                        else if (refs[i]->m_frameOrder > ltrFrame->m_frameOrder) {
                            ltrFrame = refs[i];
                            ltridx = i;
                        }
                    }
                }
                if (ltrFrame && ltridx > 1) {
                    std::rotate(refs + 1, refs + ltridx, refs + ltridx + 1);
                    std::rotate(mod + 1, mod + ltridx, mod + ltridx + 1);
                    std::rotate(pocs + 1, pocs + ltridx, pocs + ltridx + 1);
                    std::rotate(lterms + 1, lterms + ltridx, lterms + ltridx + 1);
                    list.m_listModFlag = 1;
                }
            }
        }
    }
}

int32_t CountUniqueRefs(const RefPicList refLists[2]) {
    int32_t numUniqRefs = refLists[0].m_refFramesCount;
    for (int32_t r = 0; r < refLists[1].m_refFramesCount; r++)
        if (std::find(refLists[0].m_refFrames, refLists[0].m_refFrames + refLists[0].m_refFramesCount, refLists[1].m_refFrames[r])
            == refLists[0].m_refFrames + refLists[0].m_refFramesCount)
            numUniqRefs++;
    return numUniqRefs;
}

struct AV1ShortTermRefPicSet
{
    uint8_t  inter_ref_pic_set_prediction_flag;
    int32_t delta_idx;
    uint8_t  delta_rps_sign;
    int32_t abs_delta_rps;
    uint8_t  use_delta_flag[MAX_DPB_SIZE];
    uint8_t  num_negative_pics;
    uint8_t  num_positive_pics;
    int32_t delta_poc[MAX_DPB_SIZE];             // negative + positive
    uint8_t  used_by_curr_pic_flag[MAX_DPB_SIZE]; // negative + positive
};

void AV1Encoder::CreateRefPicList(Frame *in, AV1ShortTermRefPicSet *rps)
{
    Frame *currFrame = in;
    Frame **list0 = currFrame->m_refPicList[0].m_refFrames;
    Frame **list1 = currFrame->m_refPicList[1].m_refFrames;
    int32_t currPoc = currFrame->m_poc;
    int8_t *dpoc0 = currFrame->m_refPicList[0].m_deltaPoc;
    int8_t *dpoc1 = currFrame->m_refPicList[1].m_deltaPoc;

    int32_t numStBefore = 0;
    int32_t numStAfter = 0;

    // constuct initial reference lists
    in->m_dpbSize = 0;

    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
        Frame *ref = (*i);
        if (currFrame->m_frameOrder > currFrame->m_frameOrderOfLastIntraInEncOrder &&
            ref->m_frameOrder < currFrame->m_frameOrderOfLastIntraInEncOrder)
            continue; // trailing pictures can't predict from leading pictures

        if (currFrame->m_frameOrder > m_frameOrderOfErrorResilienceFrame &&
            ref->m_frameOrder < m_frameOrderOfErrorResilienceFrame && !ref->m_isLtrFrame)
            continue; // Error Resilience

        if (currFrame->m_hasRefCtrl && m_videoParam.GopRefDist == 1) {
            int reject = 0;
            for (int j = 0; j < 16 && currFrame->refctrl.RejectedRefList[j].FrameOrder != 0xffffffff; j++) {
                if ((int32_t)currFrame->refctrl.RejectedRefList[j].FrameOrder == ref->m_frameOrder)
                    reject = 1;
            }
            if (reject) {
                if (ref->m_isExternalLTR) {
                    m_frameOrderOfLastExternalLTR[(int)ref->m_isExternalLTR - 1] = MFX_FRAMEORDER_UNKNOWN;
                    m_numberOfExternalLtrFrames--; // being rejected
                }
                continue; // Reject & Remove
            }
        }

        if (currFrame->m_shortTermRefFlag == 2 && ref->m_isLtrFrame && !ref->m_isExternalLTR)
            continue;

        in->m_dpb[in->m_dpbSize] = ref;
        in->m_dpbSize++;

        if (ref->m_poc < currFrame->m_poc)
            list0[numStBefore++] = ref;
        else
            list1[numStAfter++] = ref;
    }

    std::copy(m_actualDpbVP9, m_actualDpbVP9+NUM_REF_FRAMES, in->m_dpbVP9);

    std::sort(list0, list0 + numStBefore, PocIsGreater);
    std::sort(list1, list1 + numStAfter,  PocIsLess);

    // merge lists
    small_memcpy(list0 + numStBefore, list1, sizeof(*list0) * numStAfter);
    small_memcpy(list1 + numStAfter,  list0, sizeof(*list0) * numStBefore);

    // setup delta pocs and long-term flags
    memset(currFrame->m_refPicList[0].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[0].m_isLongTermRef));
    memset(currFrame->m_refPicList[1].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[1].m_isLongTermRef));
    for (int32_t i = 0; i < numStBefore + numStAfter; i++) {
        dpoc0[i] = int8_t(currPoc - list0[i]->m_poc);
        dpoc1[i] = int8_t(currPoc - list1[i]->m_poc);
    }

    // cut lists
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_I) {
        currFrame->m_refPicList[0].m_refFramesCount = 0;
        currFrame->m_refPicList[1].m_refFramesCount = 0;
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_P) {
        currFrame->m_refPicList[0].m_refFramesCount = std::min(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[0]);
        currFrame->m_refPicList[1].m_refFramesCount = std::min(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[1]);
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        if(m_videoParam.NumRefLayers>2 && m_videoParam.MaxRefIdxB[0]>1) {
            int32_t refCount=0, refWindow=0;
            for(int32_t j=0; j<numStBefore; j++) {
                if(currFrame->m_refPicList[0].m_refFrames[j]->m_pyramidLayer<=m_videoParam.refLayerLimit[currFrame->m_pyramidLayer]) {
                    refCount++;
                    if(refCount<=m_videoParam.MaxRefIdxB[0]) refWindow = j;
                }
            }
            currFrame->m_refPicList[0].m_refFramesCount = std::min(numStBefore + numStAfter, std::max(refWindow+1, m_videoParam.MaxRefIdxB[0]));
            currFrame->m_refPicList[1].m_refFramesCount = std::min(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[1]);
        }
        else
        {
            currFrame->m_refPicList[0].m_refFramesCount = std::min(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[0]);
            currFrame->m_refPicList[1].m_refFramesCount = std::min(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[1]);
        }
    }

    const int32_t MAX_UNIQ_REFS = 4;
    int32_t numUniqRefs = CountUniqueRefs(currFrame->m_refPicList);
    while (numUniqRefs > MAX_UNIQ_REFS) {
        (currFrame->m_refPicList[0].m_refFramesCount > currFrame->m_refPicList[1].m_refFramesCount)
            ? currFrame->m_refPicList[0].m_refFramesCount--
            : currFrame->m_refPicList[1].m_refFramesCount--;
        numUniqRefs = CountUniqueRefs(currFrame->m_refPicList);
    }

    // create RPS syntax
    // int32_t numL0 = currFrame->m_refPicList[0].m_refFramesCount;
    // int32_t numL1 = currFrame->m_refPicList[1].m_refFramesCount;
    rps->inter_ref_pic_set_prediction_flag = 0;
    rps->num_negative_pics = (uint8_t)numStBefore;
    rps->num_positive_pics = (uint8_t)numStAfter;
    int32_t deltaPocPred = 0;
    for (int32_t i = 0; i < numStBefore; i++) {
        rps->delta_poc[i] = dpoc0[i] - deltaPocPred;
        //rps->used_by_curr_pic_flag[i] = i < numL0 || i < std::max(0, numL1 - numStAfter);
        rps->used_by_curr_pic_flag[i] = (currFrame->m_picCodeType != MFX_FRAMETYPE_I); // mimic current behavior
        deltaPocPred = dpoc0[i];
    }
    deltaPocPred = 0;
    for (int32_t i = 0; i < numStAfter; i++) {
        rps->delta_poc[numStBefore + i] = deltaPocPred - dpoc1[i];
        //rps->used_by_curr_pic_flag[numStBefore + i] = i < numL1 || i < std::max(0, numL0 - numStBefore);
        rps->used_by_curr_pic_flag[numStBefore + i] = (currFrame->m_picCodeType != MFX_FRAMETYPE_I);
        deltaPocPred = dpoc1[i];
    }

    ModifyRefList(m_videoParam, *currFrame, 0);
    ModifyRefList(m_videoParam, *currFrame, 1);

    // VP9 specific
    if (currFrame->m_refPicList[0].m_refFramesCount > 0) {
        const RefPicList &rpl0 = currFrame->m_refPicList[0];
        const RefPicList &rpl1 = currFrame->m_refPicList[1];

        int32_t idx = int32_t(std::find(m_actualDpbVP9, m_actualDpbVP9+NUM_REF_FRAMES, rpl0.m_refFrames[0]) - m_actualDpbVP9);
        assert(idx < NUM_REF_FRAMES);
        currFrame->refFrameIdx[LAST_FRAME] = idx;
        currFrame->refFrameSignBiasVp9[LAST_FRAME] = (rpl0.m_refFrames[0]->m_frameOrder > currFrame->m_frameOrder);

        if (rpl0.m_refFramesCount > 1 && (currFrame->m_picCodeType == MFX_FRAMETYPE_P || rpl0.m_deltaPoc[1] > 0)) {
            int golden = 1;
            assert(!rpl0.m_refFrames[golden]->IsNotRef());
            idx = int32_t(std::find(m_actualDpbVP9, m_actualDpbVP9 + NUM_REF_FRAMES, rpl0.m_refFrames[golden]) - m_actualDpbVP9);
            assert(idx < NUM_REF_FRAMES);
            currFrame->refFrameIdx[GOLDEN_FRAME] = idx;
            currFrame->refFrameSignBiasVp9[GOLDEN_FRAME] = GetRelativeDist(m_videoParam.seqParams.orderHintBits, rpl0.m_refFrames[golden]->curFrameOffset, currFrame->curFrameOffset) > 0 ? 1 : 0;
        } else {
            currFrame->refFrameIdx[GOLDEN_FRAME] = currFrame->refFrameIdx[LAST_FRAME];
            currFrame->refFrameSignBiasVp9[GOLDEN_FRAME] = currFrame->refFrameSignBiasVp9[LAST_FRAME];
        }
        if (rpl1.m_refFramesCount > 0 && rpl1.m_deltaPoc[0] < 0) {
            idx = int32_t(std::find(m_actualDpbVP9, m_actualDpbVP9+NUM_REF_FRAMES, rpl1.m_refFrames[0]) - m_actualDpbVP9);
            assert(idx < NUM_REF_FRAMES);
            currFrame->refFrameIdx[ALTREF_FRAME] = idx;
            currFrame->refFrameSignBiasVp9[ALTREF_FRAME] = (rpl1.m_refFrames[0]->m_frameOrder > currFrame->m_frameOrder);
        } else {
            currFrame->refFrameIdx[ALTREF_FRAME] = currFrame->refFrameIdx[GOLDEN_FRAME];
            currFrame->refFrameSignBiasVp9[ALTREF_FRAME] = currFrame->refFrameSignBiasVp9[GOLDEN_FRAME];
        }

        if (currFrame->refFrameSignBiasVp9[LAST_FRAME] == currFrame->refFrameSignBiasVp9[GOLDEN_FRAME])
        {
            currFrame->compFixedRef = ALTREF_FRAME;
            currFrame->compVarRef[0] = LAST_FRAME;
            currFrame->compVarRef[1] = GOLDEN_FRAME;
        } else if (currFrame->refFrameSignBiasVp9[LAST_FRAME] == currFrame->refFrameSignBiasVp9[ALTREF_FRAME]) {
            currFrame->compFixedRef = GOLDEN_FRAME;
            currFrame->compVarRef[0] = LAST_FRAME;
            currFrame->compVarRef[1] = ALTREF_FRAME;
        } else {
            currFrame->compFixedRef = LAST_FRAME;
            currFrame->compVarRef[0] = GOLDEN_FRAME;
            currFrame->compVarRef[1] = ALTREF_FRAME;
        }
    }

    currFrame->refFramesVp9[LAST_FRAME]   = m_actualDpbVP9[currFrame->refFrameIdx[LAST_FRAME]];
    currFrame->refFramesVp9[GOLDEN_FRAME] = m_actualDpbVP9[currFrame->refFrameIdx[GOLDEN_FRAME]];
    currFrame->refFramesVp9[ALTREF_FRAME] = m_actualDpbVP9[currFrame->refFrameIdx[ALTREF_FRAME]];

    for (int32_t i = AV1_LAST_FRAME; i < INTER_REFS_PER_FRAME; i++)
        currFrame->refFramesAv1[i] = currFrame->refFramesVp9[av1_to_vp9_ref_frame_mapping[i]];

    if (!currFrame->IsIntra())
        for (int32_t i = AV1_LAST_FRAME; i < INTER_REFS_PER_FRAME; i++)
            currFrame->refFrameSide[i] =
            currFrame->refFramesAv1[i]->curFrameOffset > currFrame->curFrameOffset ? 1 : -1;
    else
        for (int32_t i = AV1_LAST_FRAME; i < INTER_REFS_PER_FRAME; i++)
            currFrame->refFrameSide[i] = -1;

    currFrame->refFrameSignBiasAv1[AV1_LAST_FRAME] = 0;
    currFrame->refFrameSignBiasAv1[AV1_LAST2_FRAME] = 0;
    currFrame->refFrameSignBiasAv1[AV1_LAST3_FRAME] = 0;
    currFrame->refFrameSignBiasAv1[AV1_GOLDEN_FRAME] = currFrame->refFrameSignBiasVp9[GOLDEN_FRAME];
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        currFrame->refFrameSignBiasAv1[AV1_BWDREF_FRAME] = 1;
        currFrame->refFrameSignBiasAv1[AV1_ALTREF2_FRAME] = 1;
        currFrame->refFrameSignBiasAv1[AV1_ALTREF_FRAME] = 1;
    }
    else {
        currFrame->refFrameSignBiasAv1[AV1_BWDREF_FRAME] = 0;
        currFrame->refFrameSignBiasAv1[AV1_ALTREF2_FRAME] = 0;
        currFrame->refFrameSignBiasAv1[AV1_ALTREF_FRAME] = currFrame->refFrameSignBiasVp9[ALTREF_FRAME];
    }

    if (currFrame->m_picCodeType == MFX_FRAMETYPE_P && m_videoParam.GeneralizedPB) {
        currFrame->isUniq[LAST_FRAME] = currFrame->refFramesVp9[LAST_FRAME] != NULL;
        currFrame->isUniq[ALTREF_FRAME] = currFrame->refFramesVp9[ALTREF_FRAME] != currFrame->refFramesVp9[LAST_FRAME];
        currFrame->isUniq[GOLDEN_FRAME] = currFrame->refFramesVp9[GOLDEN_FRAME] != currFrame->refFramesVp9[LAST_FRAME] &&
            currFrame->refFramesVp9[GOLDEN_FRAME] != currFrame->refFramesVp9[ALTREF_FRAME];
    } else {
        currFrame->isUniq[LAST_FRAME] = currFrame->refFramesVp9[LAST_FRAME] != NULL;
        currFrame->isUniq[GOLDEN_FRAME] = currFrame->refFramesVp9[GOLDEN_FRAME] != currFrame->refFramesVp9[LAST_FRAME];
        currFrame->isUniq[ALTREF_FRAME] = currFrame->refFramesVp9[ALTREF_FRAME] != currFrame->refFramesVp9[LAST_FRAME] &&
            currFrame->refFramesVp9[ALTREF_FRAME] != currFrame->refFramesVp9[GOLDEN_FRAME];
    }

    currFrame->m_temporalSync = 0;
    if (!currFrame->IsIntra()) {
        for (int8_t refIdx = LAST_FRAME; refIdx <= ALTREF_FRAME; refIdx++) {
            if (!currFrame->isUniq[refIdx])
                continue;
            if (currFrame->m_sceneOrder == currFrame->refFramesVp9[refIdx]->m_sceneOrder)
                currFrame->m_temporalSync = 1;
        }
        //if (!currFrame->m_temporalSync) printf("\nPOC %d Temporal Sync Off %d %d %d\n", currFrame->m_poc, currFrame->m_sceneOrder, currFrame->refFramesVp9[0]->m_sceneOrder, currFrame->refFramesVp9[2]->m_sceneOrder);
        //else                            printf("\nPOC %d Temporal Sync On %d %d %d\n", currFrame->m_poc, currFrame->m_sceneOrder, currFrame->refFramesVp9[0]->m_sceneOrder, currFrame->refFramesVp9[2]->m_sceneOrder);
    }

    if (currFrame->m_picCodeType == MFX_FRAMETYPE_P) {
        currFrame->compoundReferenceAllowed = 0;
        if(m_videoParam.GeneralizedPB && currFrame->isUniq[ALTREF_FRAME])
            currFrame->compoundReferenceAllowed = 1;
    } else {
        currFrame->compoundReferenceAllowed =
            currFrame->refFrameSignBiasVp9[LAST_FRAME] != currFrame->refFrameSignBiasVp9[GOLDEN_FRAME] ||
            currFrame->refFrameSignBiasVp9[LAST_FRAME] != currFrame->refFrameSignBiasVp9[ALTREF_FRAME];
    }
}


uint8_t SameRps(const AV1ShortTermRefPicSet *rps1, const AV1ShortTermRefPicSet *rps2)
{
    // check number of refs
    if (rps1->num_negative_pics != rps2->num_negative_pics ||
        rps1->num_positive_pics != rps2->num_positive_pics)
        return 0;
    // check delta pocs and "used" flags
    for (int32_t i = 0; i < rps1->num_negative_pics + rps1->num_positive_pics; i++)
        if (rps1->delta_poc[i] != rps2->delta_poc[i] ||
            rps1->used_by_curr_pic_flag[i] != rps2->used_by_curr_pic_flag[i])
            return 0;
    return 1;
}

//struct isEqual
//{
//    isEqual(int32_t frameOrder): m_frameOrder(frameOrder)
//    {}
//
//    template<typename T>
//    bool operator()(const T& l)
//    {
//        return (*l).m_frameOrder == m_frameOrder;
//    }
//
//    int32_t m_frameOrder;
//};

void AV1Encoder::ConfigureEncodeFrame(Frame* frame)
{
    Frame *currFrame = frame;

    // update frame order of most recent intra frame in encoding order
    currFrame->m_frameOrderOfLastIntraInEncOrder = m_frameOrderOfLastIntraInEncOrder;
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_I)
        m_frameOrderOfLastIntraInEncOrder = currFrame->m_frameOrder;

    // setup ref flags
    currFrame->m_isShortTermRef = 1;
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        if (m_videoParam.BiPyramidLayers > 1) {
            if (currFrame->m_pyramidLayer == m_videoParam.BiPyramidLayers - 1)
                currFrame->m_isShortTermRef = 0;
        }
    }
    currFrame->m_isRef = currFrame->m_isShortTermRef;
#if defined(PARALLEL_LOW_DELAY)
    // if we use Parallel Low delay P-pyramid (no B-frames) we mark insignificant frames as non-ref
    // it impact on ref list construction, post-processing etc
    // and must be consistent
    if (m_videoParam.GopRefDist == 1 && m_videoParam.PGopPicSize>1) {
        int32_t parallelLowDelay = MAX(0, MIN(2, (int)m_videoParam.NumRefLayers-1));
        if ((parallelLowDelay && currFrame->m_pyramidLayer == 2) ||
            (!parallelLowDelay && currFrame->m_pyramidLayer == 2 && m_videoParam.SceneCut && !currFrame->m_temporalActv && (currFrame->m_sceneStats->RepeatPattern&0x3)!=0x2)) {
            //if(!parallelLowDelay) printf("Fr %d L2 of %dL NR\n", currFrame->m_frameOrder, parallelLowDelay);
            currFrame->m_isRef = 0;
        }
    }
#endif

    // create reference lists and RPS syntax
    AV1ShortTermRefPicSet rps = {0};
    CreateRefPicList(currFrame, &rps);

    if (currFrame->m_isErrorResilienceFrame) {
        m_frameOrderOfErrorResilienceFrame = currFrame->m_frameOrder;
    }

    currFrame->m_encOrder = m_lastEncOrder + 1;

    currFrame->interpolationFilter = m_videoParam.interpolationFilter;

    // set task param to simplify bitstream writing logic
    currFrame->m_frameType = currFrame->m_picCodeType |
        (currFrame->m_isIdrPic ? MFX_FRAMETYPE_IDR : 0) |
        (currFrame->m_isRef ? MFX_FRAMETYPE_REF : 0);


    if (currFrame->compoundReferenceAllowed) {
        currFrame->referenceMode = REFERENCE_MODE_SELECT;
    } else {
        currFrame->referenceMode = (currFrame->m_picCodeType == MFX_FRAMETYPE_B) ? REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
    }


    // take ownership over reference frames
    for (int32_t i = 0; i < currFrame->m_dpbSize; i++)
        currFrame->m_dpb[i]->AddRef();

    UpdateDpb(currFrame);

    // hint from lookahead
    //if (currFrame->m_slices[0].NalUnitType != NAL_RASL_R && currFrame->m_slices[0].NalUnitType != NAL_RASL_N) {
    //    currFrame->m_forceTryIntra = 0;
    //}

    currFrame->m_prevFrame = NULL;
    //if (m_prevFrame == NULL ||
    //    m_prevFrame != currFrame->refFramesVp9[LAST_FRAME] ||
    //    m_prevFrame != currFrame->refFramesVp9[GOLDEN_FRAME] ||
    //    m_prevFrame != currFrame->refFramesVp9[ALTREF_FRAME])
    //{
    //    SafeRelease(m_prevFrame);
    //    m_prevFrame = currFrame->refFramesVp9[LAST_FRAME];
    //    if (m_prevFrame)
    //        m_prevFrame->AddRef();
    //}

    //currFrame->m_prevFrameIsOneOfRefs = 1;

    //if (!m_videoParam.errorResilientMode && !currFrame->IsIntra() &&
    //    m_lastShowFrame && m_prevFrame && !m_prevFrame->IsIntra())
    //{
    //    currFrame->m_prevFrame = m_prevFrame;
    //    currFrame->m_prevFrame->AddRef();
    //}

    //m_lastShowFrame = currFrame->showFrame;
    //if (currFrame->m_isRef) {
    //    SafeRelease(m_prevFrame);
    //    m_prevFrame = currFrame;
    //    m_prevFrame->AddRef();
    //}
}

// expect that frames in m_dpb is ordered by m_encOrder
void AV1Encoder::UpdateDpb(Frame *currFrame)
{
    currFrame->refreshFrameFlags = 0;
    // Find LTR Frame to Keep
    Frame *ltrFrame = NULL;
    //Frame *ltrFrameExt = NULL;
    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
        Frame *ref = (*i);
        if (ref->m_isLtrFrame) {
            if (!ltrFrame) {
                ltrFrame = ref;
            } else if (ref->m_frameOrder > ltrFrame->m_frameOrder) {
                ltrFrame = ref;
            }
            /*if (ref->m_isExternalLTR) {
                if (!ltrFrameExt) {
                    ltrFrameExt = ref;
                } else if (ref->m_frameOrder > ltrFrameExt->m_frameOrder) {
                    ltrFrameExt = ref;
                }
            }*/
        }
    }

    if (currFrame->m_isRef) {
        // first of all remove frames not appeared in current frame RPS
        for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end();) {

            bool found = std::find(currFrame->m_dpb, currFrame->m_dpb + currFrame->m_dpbSize, *i)
                != currFrame->m_dpb + currFrame->m_dpbSize;

            FrameIter toRemove = i++;
            if (!found) {
                // VP9 specific
                for (int32_t j = 0; j < NUM_REF_FRAMES; j++) {
                    if (m_actualDpbVP9[j] == *toRemove) {
                        m_actualDpbVP9[j] = currFrame;
                        currFrame->refreshFrameFlags |= 1 << j;
                    }
                }
                //printf("\nCurr %d Remove Not Found %d\n", currFrame->m_frameOrder, (*toRemove)->m_frameOrder);
                (*toRemove)->Release();
                m_actualDpb.erase(toRemove);
            }
        }

        if (currFrame->m_isIdrPic) {
            // IDR frame removes all reference frames from dpb and becomes the only reference
            for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                (*i)->Release();
            m_actualDpb.clear();
            Zero(m_actualDpbVP9);
            std::fill(m_actualDpbVP9, m_actualDpbVP9 + NUM_REF_FRAMES, currFrame);
            currFrame->refreshFrameFlags = 0xff;
        }
        else if ((int32_t)m_actualDpb.size() == m_videoParam.MaxDecPicBuffering) {
            // dpb is full
            // need to make a place in dpb for a new reference frame
            FrameIter toRemove = m_actualDpb.end();
            if (m_videoParam.GopRefDist > 1) {
                // B frames with or without pyramid
                // search for oldest refs from previous minigop
                int32_t currMiniGop = currFrame->m_miniGopCount;
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                    if ((*i)->m_miniGopCount < currMiniGop)
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                            toRemove = i;
                if (toRemove == m_actualDpb.end()) {
                    // if nothing found
                    // search for oldest B frame in current minigop
                    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                        if ((*i)->m_picCodeType == MFX_FRAMETYPE_B &&
                            (*i)->m_miniGopCount == currMiniGop)
                            if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                                toRemove = i;
                }
                if (toRemove == m_actualDpb.end()) {
                    // if nothing found
                    // search for any oldest ref in current minigop
                    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
                        if ((*i)->m_miniGopCount == currMiniGop)
                            if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                                toRemove = i;
                }
            }
            else {
                int32_t refLayer = 0;
                if (m_videoParam.NumRefLayers <= 2) refLayer = 1;
                if (m_videoParam.NumRefLayers == 1 && currFrame->m_pyramidLayer == 2 && currFrame->m_isRef)
                    refLayer = 0;

                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    if ((*i)->m_pyramidLayer > refLayer) {
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                            toRemove = i;
                    }
                }
                // P frames with or without pyramid
                // search for oldest non-anchor ref
                if (toRemove == m_actualDpb.end()) {
                    // if nothing found
                    // search for any oldest ref
                    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                        if ((toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                            && *i != ltrFrame
                            && (!(*i)->m_isExternalLTR || m_frameOrderOfLastExternalLTR[(int)(*i)->m_isExternalLTR - 1] != (*i)->m_frameOrder))
                            toRemove = i;
                    }
                    if (toRemove == m_actualDpb.end()) {
                        toRemove = m_actualDpb.begin();
                    }
                }
            }

            for (int32_t i = 0; i < NUM_REF_FRAMES; i++) {
                if (m_actualDpbVP9[i] == *toRemove) {
                    m_actualDpbVP9[i] = currFrame;
                    currFrame->refreshFrameFlags |= 1<<i;
                }
            }
            //printf("\nCurr %d Remove %d\n", currFrame->m_frameOrder, (*toRemove)->m_frameOrder);
            assert(toRemove != m_actualDpb.end());
            (*toRemove)->Release();
            m_actualDpb.erase(toRemove);
        } else {
            bool inserted = false;
            for (int32_t i = 0; i < NUM_REF_FRAMES; i++) {
                if (m_actualDpbVP9[i] == NULL) {
                    m_actualDpbVP9[i] = currFrame;
                    currFrame->refreshFrameFlags |= 1<<i;
                    inserted = true;
                }
            }
            if (!inserted) { // look for duplicates
                for (int32_t i = 0; i < NUM_REF_FRAMES && !inserted; i++) {
                    if (std::find(m_actualDpbVP9+i+1, m_actualDpbVP9+NUM_REF_FRAMES, m_actualDpbVP9[i]) != m_actualDpbVP9+NUM_REF_FRAMES) {
                        m_actualDpbVP9[i] = currFrame;
                        currFrame->refreshFrameFlags |= 1<<i;
                        inserted = true;
                        break;
                    }
                }
                assert(inserted);
            }
        }

        assert((int32_t)m_actualDpb.size() < m_videoParam.MaxDecPicBuffering);
        m_actualDpb.push_back(currFrame);
        currFrame->AddRef();
    }

    std::copy(m_actualDpbVP9, m_actualDpbVP9+NUM_REF_FRAMES, currFrame->m_dpbVP9NextFrame);

    for (int32_t i = 0; i < NUM_REF_FRAMES; i++) {
        currFrame->m_refFrameId[i] = m_refFrameId[i];
        if ((currFrame->refreshFrameFlags >> i) & 1) {
            m_refFrameId[i] = currFrame->m_encOrder;
        }
    }
}


void AV1Encoder::CleanGlobalDpb()
{
    for (FrameIter i = m_dpb.begin(); i != m_dpb.end();) {
        FrameIter curI = i++;
        if ((*curI)->m_refCounter.load() == 0) {
            SafeRelease((*curI)->m_recon);
            SafeRelease((*curI)->m_reconUpscale);
            SafeRelease((*curI)->m_feiRecon);
            if (m_videoParam.src10Enable) {
                SafeRelease((*curI)->m_recon10);
            }
#if GPU_VARTX
            SafeRelease((*curI)->m_feiAdz);
            SafeRelease((*curI)->m_feiAdzDelta);
#endif // GPU_VARTX
            m_free.splice(m_free.end(), m_dpb, curI);
        }
    }
}

void AV1Encoder::OnEncodingQueried(Frame* encoded)
{
    Dump(&m_videoParam, encoded, m_dpb);

    // release reference frames
    for (int32_t i = 0; i < encoded->m_dpbSize; i++)
        encoded->m_dpb[i]->Release();

    // release source frame
    SafeRelease(encoded->m_origin);

    if (m_videoParam.CmInterpFlag) {
        SafeRelease(encoded->m_recon);
    }

    if (m_videoParam.src10Enable) {
        SafeRelease(encoded->m_origin10);
    }
    SafeRelease(encoded->m_feiOrigin);
    SafeRelease(encoded->m_feiModeInfo1);
    SafeRelease(encoded->m_feiModeInfo2);
#if GPU_VARTX
    SafeRelease(encoded->m_feiVarTxInfo);
    SafeRelease(encoded->m_feiCoefs);
#endif // GPU_VARTX
    for (int i = 0; i < 4; i++) {
        SafeRelease(encoded->m_feiRs[i]);
        SafeRelease(encoded->m_feiCs[i]);
    }

    // release FEI resources
    for (int32_t i = 0; i < 3; i++)
        for (int32_t j = 0; j < 4; j++)
            SafeRelease(encoded->m_feiInterData[i][j]);

    SafeRelease(encoded->m_lowres);
    SafeRelease(encoded->m_prevFrame);

    if (m_la.get()) {
        for (int32_t idx = 0; idx < 2; idx++)
            SafeRelease(encoded->m_stats[idx]);
        SafeRelease(encoded->m_sceneStats);
    }
    // release encoded frame
    encoded->Release();

    m_outputQueue.remove(encoded);
    CleanGlobalDpb();
}



class OnExitHelper
{
public:
    OnExitHelper(uint32_t* arg) : m_arg(arg)
    {}
    ~OnExitHelper()
    {
        if (m_arg)
            vm_interlocked_cas32(reinterpret_cast<volatile uint32_t *>(m_arg), 0, 1);
    }

private:
    uint32_t* m_arg;
};


const int32_t MAX_OFFSET_WIDTH = 64;
const int32_t MAX_OFFSET_HEIGHT = 0;

int get_block_position(int32_t miRows, int32_t miCols, int *mi_r, int *mi_c, int blk_row, int blk_col, AV1MV mv, int sign_bias)
{
    const int base_blk_row = (blk_row >> 3) << 3;
    const int base_blk_col = (blk_col >> 3) << 3;

    const int row_offset = (mv.mvy >= 0)
        ? (mv.mvy >> (4 + MI_SIZE_LOG2))
        : -((-mv.mvy) >> (4 + MI_SIZE_LOG2));

    const int col_offset = (mv.mvx >= 0)
        ? (mv.mvx >> (4 + MI_SIZE_LOG2))
        : -((-mv.mvx) >> (4 + MI_SIZE_LOG2));

    int row = (sign_bias == 1) ? blk_row - row_offset : blk_row + row_offset;
    int col = (sign_bias == 1) ? blk_col - col_offset : blk_col + col_offset;

    if (row < 0 || row >= miRows || col < 0 || col >= miCols)
        return 0;

    if (row <= base_blk_row - (MAX_OFFSET_HEIGHT >> 3) ||
        row >= base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3) ||
        col <= base_blk_col - (MAX_OFFSET_WIDTH >> 3) ||
        col >= base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3))
        return 0;

    *mi_r = row;
    *mi_c = col;

    return 1;
}

int32_t motion_field_projection(const AV1VideoParam &par, Frame *frame, int32_t ref_frame, int32_t ref_stamp, int32_t dir)
{
    const int32_t orderBits = par.seqParams.orderHintBits;
    int cur_rf_index[INTER_REFS_PER_FRAME] = { 0 };
    int cur_offset[INTER_REFS_PER_FRAME] = { 0 };
    int ref_offset[INTER_REFS_PER_FRAME] = { 0 };

    (void)dir;

    if (frame->refFramesAv1[ref_frame]->IsIntra())
        return 0;

    Frame **refFrames = frame->refFramesAv1;
    Frame **refRefFrames = frame->refFramesAv1[ref_frame]->refFramesAv1;
    int ref_frame_index = frame->refFramesAv1[ref_frame]->curFrameOffset;
    int cur_frame_index = frame->curFrameOffset;
    int ref_to_cur = GetRelativeDist(orderBits, ref_frame_index, cur_frame_index);

    for (RefIdx rf = LAST_FRAME; rf < INTER_REFS_PER_FRAME; ++rf) {
        cur_rf_index[rf] = refFrames[rf]->curFrameOffset;
        cur_offset[rf] = GetRelativeDist(orderBits, cur_frame_index, refFrames[rf]->curFrameOffset);
        ref_offset[rf] = GetRelativeDist(orderBits, ref_frame_index, refRefFrames[rf]->curFrameOffset);
    }

    if (dir == 1) {
        ref_to_cur = -ref_to_cur;
        for (RefIdx rf = LAST_FRAME; rf < INTER_REFS_PER_FRAME; ++rf) {
            cur_offset[rf] = -cur_offset[rf];
            ref_offset[rf] = -ref_offset[rf];
        }
    }

    if (dir == 2)
        ref_to_cur = -ref_to_cur;

    ModeInfo *mv_ref_base = refFrames[ref_frame]->m_modeInfo;
    int8_t *refRefFrameSize = refFrames[ref_frame]->refFrameSide;

    for (int blk_row = 0; blk_row < par.miRows; ++blk_row) {
        for (int blk_col = 0; blk_col < par.miCols; ++blk_col) {
            ModeInfo *mv_ref = &mv_ref_base[blk_row * par.miPitch + blk_col];
            RefIdx refRefIdx = mv_ref->refIdx[dir & 0x01];
            if (refRefIdx > INTRA_FRAME) {
                refRefIdx = vp9_to_av1_ref_frame_mapping[refRefIdx];
                if (refRefFrameSize[refRefIdx] < 0)
                    continue;

                //const AV1MV fwd_mv = mv_ref->mv[dir & 0x01];
                //const int ref_frame_offset = ref_offset[mv_ref->refIdx[dir & 0x01]];

                //int pos_valid = abs(ref_frame_offset) < MAX_FRAME_DISTANCE && abs(ref_to_cur) < MAX_FRAME_DISTANCE;
                //if (pos_valid) {
                //    AV1MV this_mv;
                //    int mi_r, mi_c;

                //    GetMvProjection(&this_mv, fwd_mv, ref_to_cur, ref_frame_offset);
                //    pos_valid = get_block_position(par.miRows, par.miCols, &mi_r, &mi_c, blk_row, blk_col, this_mv, dir >> 1);
                //    if (pos_valid) {
                //        int mi_offset = mi_r * (par.miPitch >> 1) + mi_c;
                //        tpl_mvs_base[mi_offset].mfmv0[ref_stamp].mvy = (dir == 1) ? -fwd_mv.mvy : fwd_mv.mvy;
                //        tpl_mvs_base[mi_offset].mfmv0[ref_stamp].mvx = (dir == 1) ? -fwd_mv.mvx : fwd_mv.mvx;
                //        tpl_mvs_base[mi_offset].ref_frame_offset[ref_stamp] = ref_frame_offset;
                //    }
                //}
            }
        }
    }

    return 1;
}
#if ENABLE_TPLMV
void av1_setup_motion_field(const AV1VideoParam &par, const TileBorders &tileBorders, Frame *frame)
{
    if (!par.seqParams.enableOrderHint || par.errorResilientMode || frame->IsIntra())
        return;

    TplMvRef *tpl_mvs_base = frame->m_fenc->m_tplMvs;
    int size = (par.miRows + 8) * par.miPitch;
    for (int idx = 0; idx < size; ++idx) {
        for (int i = 0; i < MFMV_STACK_SIZE; ++i) {
            tpl_mvs_base[idx].mfmv0[i] = INVALID_MV;
            tpl_mvs_base[idx].ref_frame_offset[i] = 0;
        }
    }

    int32_t cur_frame_index  = frame->curFrameOffset;
    int32_t bwd_frame_index  = frame->refFramesAv1[AV1_BWDREF_FRAME]->curFrameOffset;
    int32_t alt2_frame_index = frame->refFramesAv1[AV1_ALTREF2_FRAME]->curFrameOffset;
    int32_t alt_frame_index  = frame->refFramesAv1[AV1_ALTREF_FRAME]->curFrameOffset;

    int ref_stamp = MFMV_STACK_SIZE - 1;

    if (frame->refFramesAv1[AV1_ALTREF_FRAME] != frame->refFramesAv1[AV1_GOLDEN_FRAME])
        motion_field_projection(par, frame, AV1_LAST_FRAME, ref_stamp, 2);
    --ref_stamp;

    if (bwd_frame_index > cur_frame_index)
        if (motion_field_projection(par, frame, AV1_BWDREF_FRAME, ref_stamp, 0))
            --ref_stamp;

    if (alt2_frame_index > cur_frame_index)
        if (motion_field_projection(par, frame, AV1_ALTREF2_FRAME, ref_stamp, 0))
            --ref_stamp;

    if (alt_frame_index > cur_frame_index && ref_stamp >= 0)
        if (motion_field_projection(par, frame, AV1_ALTREF_FRAME, ref_stamp, 0))
            --ref_stamp;

    if (ref_stamp >= 0)
        if (motion_field_projection(par, frame, AV1_LAST2_FRAME, ref_stamp, 2))
            --ref_stamp;
}
#endif
template <typename PixType>
mfxStatus AV1FrameEncoder::PerformThreadingTask(uint32_t ctb_row, uint32_t ctb_col)
{
    AV1VideoParam *pars = &m_videoParam;

    // assign local thread id
    int32_t ithread = -1;
    for (uint32_t idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
        uint32_t stage = vm_interlocked_cas32( & m_topEnc.m_ithreadPool[idx], 1, 0);
        if(stage == 0) {
            ithread = idx;
            break;
        }
    }
    if (ithread == -1)
        return MFX_TASK_BUSY;

    OnExitHelper exitHelper( &(m_topEnc.m_ithreadPool[ithread]) );

    AV1CU<PixType> *cu_ithread = (AV1CU<PixType> *)m_topEnc.m_cu + ithread;

    uint32_t ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;

    CoeffsType *coefs = m_coeffWork + (ctb_addr << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift);
    ModeInfo *cuData = m_frame->m_modeInfo + (ctb_col << 3) + (ctb_row << 3) * m_videoParam.miPitch;
    cu_ithread->InitCu(*pars, cuData, m_dataTemp + ithread * m_dataTempSize, ctb_row, ctb_col, *m_frame, coefs, &m_frame->m_lcuRound[ctb_addr]);

    const int32_t tile_index = GetTileIndex(m_frame->m_tileParam, ctb_row, ctb_col);
#if ENABLE_TPLMV
    if ((int32_t)ctb_row == (cu_ithread->m_tileBorders.rowStart >> 3) &&
        (int32_t)ctb_col == (cu_ithread->m_tileBorders.colStart >> 3))
        av1_setup_motion_field(*pars, cu_ithread->m_tileBorders, m_frame);
#endif

    if (m_frame->m_allowIntraBc && (int32_t)ctb_col == (cu_ithread->m_tileBorders.colStart >> 3)) {

        const TileBorders &tileBorderMi = cu_ithread->m_tileBorders;
        const int32_t tileWidth = (tileBorderMi.colEnd - tileBorderMi.colStart) * 8;
        const int32_t tileHeight = (tileBorderMi.rowEnd - tileBorderMi.rowStart) * 8;
        const int32_t alignedTileWidth = (tileWidth + 15) & ~15;
        const int32_t alignedTileHeight = (tileHeight + 15) & ~15;
        const int32_t accPitch = alignedTileWidth + 16;
#if !ENABLE_HASH_MEM_REDUCTION
        uint16_t *sum8 = m_tmpBufForHash[tile_index];
        uint16_t *sum16 = sum8 + accPitch * (alignedTileHeight + 1);
        uint16_t *hash16 = sum16 + accPitch * (alignedTileHeight + 1);
        uint16_t *hash8 = hash16 + accPitch * (alignedTileHeight + 1);
#else
        uint16_t *hash8 = m_tmpBufForHash[tile_index];
#endif

        HashTable &hashTable8 = m_frame->m_fenc->m_hash[0][tile_index];
        const int32_t startY = tileBorderMi.rowStart * 8;
        const int32_t startX = tileBorderMi.colStart * 8;

        //hashTable8.m_mutex.lock();
        {
            uint16_t *phash = hash8 - startX - startY * accPitch;
#if !ENABLE_HASH_MEM_REDUCTION
            uint16_t *psum = sum8 - startX - startY * accPitch;
#endif
            const int32_t endY = startY + tileHeight - 7;
            const int32_t endX = startX + tileWidth - 7;
            const int32_t sby = (int32_t)ctb_row * 64 - 7;
            const int32_t sbStartY = std::max(sby, startY);
            const int32_t sbEndY = std::min(sby + 64, endY);
            BlockHash::Packed2Bytes srcSb = { uint8_t(ctb_col), uint8_t(ctb_row) };
            for (int32_t sbx = startX - 7; sbx < endX; sbx += 64, ++srcSb.asInt16) {
                const int32_t sbStartX = std::max(startX, sbx);
                const int32_t sbEndX = std::min(endX, sbx + 64);
                for (int32_t y = sbStartY; y < sbEndY; y++)
                    for (int32_t x = sbStartX; x < sbEndX; x++)
                        if (phash[y * accPitch + x]) {
#if !ENABLE_HASH_MEM_REDUCTION
                            hashTable8.AddBlock(phash[y * accPitch + x], (uint16_t)x, (uint16_t)y, psum[y * accPitch + x], srcSb);
#else
                            hashTable8.AddBlock(phash[y * accPitch + x], (uint16_t)x, (uint16_t)y, srcSb);
#endif
                        }
            }
        }
        //hashTable8.m_mutex.unlock();
#if !ENABLE_ONLY_HASH8
		HashTable &hashTable16 = m_frame->m_fenc->m_hash[1][tile_index];
        //hashTable16.m_mutex.lock();
        {
            uint16_t *phash16 = hash16 - startX - startY * accPitch;
            uint16_t *psum16 = sum16 - startX - startY * accPitch;
            const int32_t endY = startY + tileHeight - 15;
            const int32_t endX = startX + tileWidth - 15;
            const int32_t sby = (int32_t)ctb_row * 64 - 15;
            const int32_t sbStartY = std::max(sby, startY);
            const int32_t sbEndY = std::min(sby + 64, endY);
            BlockHash::Packed2Bytes srcSb = { uint8_t(ctb_col), uint8_t(ctb_row) };
            for (int32_t sbx = startX - 15; sbx < endX; sbx += 64, ++srcSb.asInt16) {
                const int32_t sbStartX = std::max(startX, sbx);
                const int32_t sbEndX = std::min(endX, sbx + 64);
                for (int32_t y = sbStartY; y < sbEndY; y++)
                    for (int32_t x = sbStartX; x < sbEndX; x++)
                        if (phash16[y * accPitch + x])
                            hashTable16.AddBlock(phash16[y * accPitch + x], (uint16_t)x, (uint16_t)y, psum16[y * accPitch + x], srcSb);
            }
        }
        //hashTable16.m_mutex.unlock();
#endif
    }

    if (m_videoParam.targetUsage == 1) {
        cu_ithread->ModeDecision(0, 0);
    } else {
        if (m_frame->IsIntra()) {
            cu_ithread->ModeDecision(0, 0);
            cu_ithread->RefineDecisionIAV1();
            if (m_videoParam.src10Enable) {
                cu_ithread->MakeRecon10bitIAV1();
            }
        } else {
#if !PROTOTYPE_GPU_MODE_DECISION
            cu_ithread->ModeDecisionInterTu7<CODEC_AV1, 0>(ctb_row << 3, ctb_col << 3);
#endif // !PROTOTYPE_GPU_MODE_DECISION
            cu_ithread->RefineDecisionAV1();
            if (m_videoParam.src10Enable) {
                cu_ithread->MakeRecon10bitAV1();
            }
        }
    }

    const int32_t ctbColWithinTile = ctb_col - (cu_ithread->m_tileBorders.colStart >> 3);
    const int32_t ctbRowWithinTile = ctb_row - (cu_ithread->m_tileBorders.rowStart >> 3);
    TileContexts &tileCtx = m_frame->m_tileContexts[ tile_index ];
    As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4)) = As16B(cu_ithread->m_contexts.aboveNonzero[0]);
    As8B(tileCtx.aboveNonzero[1]  + (ctbColWithinTile << 3)) = As8B (cu_ithread->m_contexts.aboveNonzero[1]);
    As8B(tileCtx.aboveNonzero[2]  + (ctbColWithinTile << 3)) = As8B (cu_ithread->m_contexts.aboveNonzero[2]);
    As16B(tileCtx.leftNonzero[0]  + (ctbRowWithinTile << 4)) = As16B(cu_ithread->m_contexts.leftNonzero[0]);
    As8B(tileCtx.leftNonzero[1]   + (ctbRowWithinTile << 3)) = As8B (cu_ithread->m_contexts.leftNonzero[1]);
    As8B(tileCtx.leftNonzero[2]   + (ctbRowWithinTile << 3)) = As8B (cu_ithread->m_contexts.leftNonzero[2]);
    As8B(tileCtx.abovePartition   + (ctbColWithinTile << 3)) = As8B (cu_ithread->m_contexts.abovePartition);
    As8B(tileCtx.leftPartition    + (ctbRowWithinTile << 3)) = As8B (cu_ithread->m_contexts.leftPartition);
#ifdef ADAPTIVE_DEADZONE
    {
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        const int32_t DZTW = tileCtx.tileSb64Cols;
#else
        const int32_t DZTW = 3;
#endif
        ADzCtx t = tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];
        // store factors to allow qp variation
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorY[0]     = (cu_ithread->m_roundFAdjY[0] * 128.0f) / (float)cu_ithread->m_aqparamY.dequant[0]; // can division be avoided?
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorY[1]     = (cu_ithread->m_roundFAdjY[1] * 128.0f) / (float)cu_ithread->m_aqparamY.dequant[1];
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[0][0] = (cu_ithread->m_roundFAdjUv[0][0] * 128.0f) / (float)cu_ithread->m_aqparamUv[0].dequant[0];
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[0][1] = (cu_ithread->m_roundFAdjUv[0][1] * 128.0f) / (float)cu_ithread->m_aqparamUv[0].dequant[1];
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[1][0] = (cu_ithread->m_roundFAdjUv[1][0] * 128.0f) / (float)cu_ithread->m_aqparamUv[1].dequant[0];
        tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[1][1] = (cu_ithread->m_roundFAdjUv[1][1] * 128.0f) / (float)cu_ithread->m_aqparamUv[1].dequant[1];
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        // store delta factors
        ADzCtx *n = &tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];
        ADzCtx *d = &tileCtx.adzctxDelta[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];
        d->aqroundFactorY[0]     = n->aqroundFactorY[0]     - t.aqroundFactorY[0];
        d->aqroundFactorY[1]     = n->aqroundFactorY[1]     - t.aqroundFactorY[1];
        d->aqroundFactorUv[0][0] = n->aqroundFactorUv[0][0] - t.aqroundFactorUv[0][0];
        d->aqroundFactorUv[0][1] = n->aqroundFactorUv[0][1] - t.aqroundFactorUv[0][1];
        d->aqroundFactorUv[1][0] = n->aqroundFactorUv[1][0] - t.aqroundFactorUv[1][0];
        d->aqroundFactorUv[1][1] = n->aqroundFactorUv[1][1] - t.aqroundFactorUv[1][1];
#endif

#if GPU_VARTX
        ADzCtx *gpuAdz = (ADzCtx *)m_frame->m_feiAdz->m_sysmem;
        ADzCtx *gpuAdzDelta = (ADzCtx *)m_frame->m_feiAdzDelta->m_sysmem;
        //if (m_frame->IsIntra()) {
        gpuAdz[ctb_addr].aqroundFactorY[0] = n->aqroundFactorY[0];
        gpuAdz[ctb_addr].aqroundFactorY[1] = n->aqroundFactorY[1];
        gpuAdzDelta[ctb_addr].aqroundFactorY[0] = d->aqroundFactorY[0];
        gpuAdzDelta[ctb_addr].aqroundFactorY[1] = d->aqroundFactorY[1];
        //} else {
        //    n->aqroundFactorY[0] = gpuAdz[ctb_addr].aqroundFactorY[0];
        //    n->aqroundFactorY[1] = gpuAdz[ctb_addr].aqroundFactorY[1];
        //    d->aqroundFactorY[0] = gpuAdzDelta[ctb_addr].aqroundFactorY[0];
        //    d->aqroundFactorY[1] = gpuAdzDelta[ctb_addr].aqroundFactorY[1];
        //}
#endif // GPU_VARTX
        /*
        if (ctb_col == (cu_ithread->m_tileBorders.colEnd >> 3) - 1 && ctb_row == (cu_ithread->m_tileBorders.rowEnd >> 3) - 1) {
            int32_t frmclass = (m_frame->IsIntra() ? 0 : (((m_videoParam.BiPyramidLayers > 1 && m_frame->m_pyramidLayer >= 3) || m_frame->IsNotRef()) ? 2 : 1));
            printf("\nadapt poc %d tile %d frmcl %d %d %d DzF %f, %f, U %f, %f V %f, %f\n", m_frame->m_poc, tile_index, frmclass, (int)ctb_row, (int)ctb_col,
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorY[0],
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorY[1],
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[0][0],
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[0][1],
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[1][0],
                tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile].aqroundFactorUv[1][1]);
            fflush(stdout);
        }*/
    }

#endif

    As16B(tileCtx.aboveTxfm + (ctbColWithinTile << 4)) = As16B(cu_ithread->m_contexts.aboveTxfm);
    As16B(tileCtx.leftTxfm + (ctbRowWithinTile << 4)) = As16B(cu_ithread->m_contexts.leftTxfm);
    return MFX_TASK_DONE;
}


//inline
void AV1Enc::AddTaskDependency(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool, bool threaded)
{
    // if dep num exceeded
    if (!threaded)
        assert(upstream->finished == 0);
    assert(downstream->finished == 0);

#ifdef DEBUG_NTM
    //char after_all_changes_need_to_rethink_this_part[-1];
    // if dep num exceeded
    //assert(downstream->numUpstreamDependencies < MAX_NUM_DEPENDENCIES);
    if (downstream->numUpstreamDependencies < MAX_NUM_DEPENDENCIES)
        downstream->upstreamDependencies[downstream->numUpstreamDependencies.load()] = upstream;
#endif

    if (ttHubPool && upstream->numDownstreamDependencies == MAX_NUM_DEPENDENCIES) {
        assert(MAX_NUM_DEPENDENCIES > 1);
        ThreadingTask *hub = ttHubPool->Allocate();
        Copy(hub->downstreamDependencies, upstream->downstreamDependencies);
        hub->numDownstreamDependencies = upstream->numDownstreamDependencies.load();
        upstream->numDownstreamDependencies = 0;
        AddTaskDependency(hub, upstream, NULL, threaded);
        AddTaskDependency(downstream, upstream, NULL, threaded);
    } else {
        assert(upstream->numDownstreamDependencies < MAX_NUM_DEPENDENCIES);
        downstream->numUpstreamDependencies++;
        upstream->downstreamDependencies[upstream->numDownstreamDependencies.fetch_add(1)] = downstream;
    }
}

void AV1Enc::AddTaskDependencyThreaded(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool)
{
    assert(downstream->finished == 0);

    uint32_t expected = 0;
    if (upstream->finished.compare_exchange_strong(expected, 1)) {
        AddTaskDependency(downstream, upstream, ttHubPool, true);
        upstream->finished--;
    }
}


void AV1FrameEncoder::SetEncodeFrame(Frame* frame, std::deque<ThreadingTask *> *m_pendingTasks)
{
    const AV1VideoParam &par = m_videoParam;
    m_frame = frame;

    frame->m_ttPackHeader.CommonInit(TT_PACK_HEADER);
    frame->m_ttPackHeader.row = 0;
    frame->m_ttPackHeader.col = 0;
    frame->m_ttPackHeader.frame = frame;
    frame->m_ttPackHeader.poc = frame->m_frameOrder;

    ThreadingTask *ttEnc = frame->m_ttEncode;
    ThreadingTask *ttPostProc = frame->m_ttDeblockAndCdef;

    for (int32_t sbRow = 0; sbRow < par.sb64Rows; sbRow++) {
        for (int32_t sbCol = 0; sbCol < par.sb64Cols; sbCol++, ttEnc++) {
            const TileBorders tileBorders = GetTileBordersSb(frame->m_tileParam, sbRow, sbCol);
            ttEnc->CommonInit(TT_ENCODE_CTU);
            ttEnc->row = sbRow;
            ttEnc->col = sbCol;
            ttEnc->frame = frame;
            ttEnc->poc = frame->m_frameOrder;

            // guard, prevents from starting until we are done with frame
            if (sbRow == tileBorders.rowStart && sbCol == tileBorders.colStart)
                ttEnc->numUpstreamDependencies = 1;

            if (sbCol > tileBorders.colStart)
                AddTaskDependency(ttEnc, ttEnc - 1);  // starts after left SB
            if (sbRow > tileBorders.rowStart) {
                if (sbCol + 1 < tileBorders.colEnd)
                    AddTaskDependency(ttEnc, ttEnc - par.sb64Cols + 1);  // starts after above-right SB
            }

            if (frame->m_prevFrame && !frame->m_prevFrameIsOneOfRefs) {
                ThreadingTask *coloc = frame->m_prevFrame->m_ttEncode + sbRow * par.sb64Cols + sbCol;
                AddTaskDependencyThreaded(ttEnc, coloc); // co-located MV may be needed for prediction
            }
        }
    }

    // Setup postprocessing tasks (deblock + cdef)
    ttEnc = frame->m_ttEncode;
    for (int32_t sbRow = 0; sbRow < par.sb64Rows; sbRow++) {
        for (int32_t sbCol = 0; sbCol < par.sb64Cols; sbCol++, ttPostProc++, ttEnc++) {
            ttPostProc->CommonInit(TT_DEBLOCK_AND_CDEF);
            ttPostProc->row = sbRow;
            ttPostProc->col = sbCol;
            ttPostProc->frame = frame;
            ttPostProc->poc = frame->m_frameOrder;

            // Postprocessing starts after SB is encoded
            AddTaskDependency(ttPostProc, ttEnc);
            // Postprocessing starts after its left, above and left-above neighbours are postprocessed
            if (sbCol > 0)
                AddTaskDependency(ttPostProc, ttPostProc - 1); // left
            if (sbRow > 0)
                AddTaskDependency(ttPostProc, ttPostProc - par.sb64Cols); // above
        }
    }

    ThreadingTask *ttLastPostProc = frame->m_ttDeblockAndCdef + par.sb64Rows * par.sb64Cols - 1;
    AddTaskDependency(&frame->m_ttEncComplete, ttLastPostProc);  // complete frame encoding after entire frame is postprocessed
    if (par.enableCmFlag && frame->m_isRef && m_videoParam.GopPicSize != 1)
        AddTaskDependency(&frame->m_ttSubmitGpuCopyRec, ttLastPostProc);  // copy recon to GPU after frame is postprocessed

    // Setup bitpacking
    if (PACK_BY_TILES) {
        const int32_t numTiles = frame->m_tileParam.numTiles;
        for (int32_t tile = 0; tile < numTiles; tile++) {
            ThreadingTask &ttPack = frame->m_ttPackTile[tile];
            ttPack.CommonInit(TT_PACK_TILE);
            ttPack.tile = tile;
            ttPack.frame = frame;
            ttPack.poc = frame->m_frameOrder;
            int32_t offset = (tile + 1 == numTiles) ? 1 : 0;
            const int32_t tileColEnd = frame->m_tileParam.colEnd[tile] - offset;
            const int32_t tileRowEnd = par.sb64Rows;
            ThreadingTask *ttLastTilePostProc = frame->m_ttDeblockAndCdef + (par.sb64Rows - 1) * par.sb64Cols + tileColEnd;
            AddTaskDependency(&ttPack, ttLastTilePostProc);   // pack tile after deblock + cdef in the tile
            AddTaskDependency(&frame->m_ttEncComplete, &ttPack);  // complete frame encoding after all tiles are packed
            if (!frame->IsIntra())
                AddTaskDependencyThreaded(&ttPack, &frame->refFramesAv1[AV1_LAST_FRAME]->m_ttPackTile[0]);
        }
    } else {
        ThreadingTask *ttPack = frame->m_ttPackRow;

        for (int32_t tileRow = 0; tileRow < frame->m_tileParam.rows; tileRow++) {
            const int32_t tileRowStart = frame->m_tileParam.rowStart[tileRow];

            for (int32_t tileCol = 0; tileCol < frame->m_tileParam.cols; tileCol++) {
                const int32_t tileColEnd = frame->m_tileParam.colEnd[tileCol];

                for (int32_t sbRow = 0; sbRow < frame->m_tileParam.rowHeight[tileRow]; ++sbRow) {
                    ttPack->CommonInit(TT_PACK_ROW);
                    ttPack->tileRow = (uint16_t)tileRow;
                    ttPack->tileCol = (uint16_t)tileCol;
                    ttPack->sbRow = sbRow;
                    ttPack->frame = frame;
                    ttPack->poc = frame->m_frameOrder;

                    if (sbRow > 0)
                        AddTaskDependency(ttPack, ttPack - 1); // row by row

                    // pack row depends on right-bottom SB postprocessing
                    const int32_t nextSbRow = std::min(m_videoParam.sb64Rows - 1, tileRowStart + sbRow + 1);
                    const int32_t lastSbCol = std::min(m_videoParam.sb64Cols - 1, tileColEnd);
                    AddTaskDependency(ttPack, frame->m_ttDeblockAndCdef + nextSbRow * m_videoParam.sb64Cols + lastSbCol);

                    // complete frame encoding after all tiles are packed
                    AddTaskDependency(&frame->m_ttEncComplete, ttPack);

                    ++ttPack;
                }
            }
        }

        if (!frame->IsIntra()) {
            const int32_t numPackRowTasksInRefFrame = frame->m_tileParam.cols * m_videoParam.sb64Rows;
            ThreadingTask *lastPackRowTaskInRefFrame = frame->refFramesAv1[AV1_LAST_FRAME]->m_ttPackRow + numPackRowTasksInRefFrame - 1;
            AddTaskDependencyThreaded(frame->m_ttPackRow, lastPackRowTaskInRefFrame);
        }
    }

#if USE_CMODEL_PAK
    if (par.enableCmFlag && frame->m_isRef) {
        AddTaskDependency(&frame->m_ttSubmitGpuCopyRec, &frame->m_ttEncComplete);
    }
#endif

    // Setup when tile encoding starts
    for (int32_t tr = 0; tr < frame->m_tileParam.rows; tr++) {
        for (int32_t tc = 0; tc < frame->m_tileParam.cols; tc++) {
            const int32_t tileColStart = frame->m_tileParam.colStart[tc];
            const int32_t tileRowStart = frame->m_tileParam.rowStart[tr];
            ttEnc = frame->m_ttEncode + tileColStart + tileRowStart * par.sb64Cols;

            AddTaskDependencyThreaded(ttEnc, &frame->m_ttInitNewFrame);  // tiles encoding started after frame is copied to internal buffer
            if (par.enableCmFlag) {
                if (!frame->IsIntra()) {
                    int32_t gpuSliceIdx = m_videoParam.numGpuSlices - 1;
                    for (; gpuSliceIdx >= 0; --gpuSliceIdx)
                        if (m_videoParam.md2KernelSliceStart[gpuSliceIdx] <= 64 * tileRowStart)
                            break;
                    AddTaskDependency(ttEnc, &frame->m_ttWaitGpuMd[gpuSliceIdx]);  // tiles encoding started after GPU is done with ME
                }
                //else if (m_videoParam.ZeroDelay)
                //     AddTaskDependencyThreaded(ttEnc, &frame->m_ttLookahead[frame->m_numLaTasks - 1]); // MD <- TT_PREENC_END for rscs if I config at preenc_start
            } else {
                for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
                    if (frame->isUniq[i]) {
                        //AddTaskDependencyThreaded(ttEnc, &frame->refFramesVp9[i]->m_ttCdefApply);  // tiles encoding started after ref frame is deblocked
                        ttLastPostProc = frame->refFramesVp9[i]->m_ttDeblockAndCdef + par.sb64Rows * par.sb64Cols - 1;
                        AddTaskDependencyThreaded(ttEnc, ttLastPostProc, &m_topEnc.m_ttHubPool);
                    }
                }
            }
        }
    }

    if (!frame->IsIntra()) {
        // Setup additional dependencies from gpu slices to cpu tasks
        for (int32_t i = 0; i < m_videoParam.numGpuSlices; i++) {
            const int32_t sbRow = m_videoParam.md2KernelSliceStart[i] / 64;
            const int32_t tr = frame->m_tileParam.mapSb2TileRow[sbRow];

            if (sbRow == frame->m_tileParam.rowStart[tr]) {
                // dependency already set
            } else {
                for (int32_t tc = 0; tc < frame->m_tileParam.cols; tc++) {
                    ttEnc = frame->m_ttEncode + sbRow * par.sb64Cols + frame->m_tileParam.colStart[tc];
                    AddTaskDependency(ttEnc, frame->m_ttWaitGpuMd + i);  // tiles encoding started after GPU is done with ME
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> guard(m_topEnc.m_critSect);
        for (int32_t tr = 0; tr < frame->m_tileParam.rows; tr++) {
            for (int32_t tc = 0; tc < frame->m_tileParam.cols; tc++) {
                const int32_t tileColStart = frame->m_tileParam.colStart[tc];
                const int32_t tileRowStart = frame->m_tileParam.rowStart[tr];
                ttEnc = frame->m_ttEncode + tileColStart + tileRowStart * par.sb64Cols;
                if (ttEnc->numUpstreamDependencies.fetch_sub(1) == 1) // remove guard
                    m_pendingTasks->push_back(ttEnc);
            }
        }
    }
}


template mfxStatus AV1FrameEncoder::PerformThreadingTask<uint8_t>(uint32_t ctb_row, uint32_t ctb_col);
#if ENABLE_10BIT
template mfxStatus AV1FrameEncoder::PerformThreadingTask<uint16_t>(uint32_t ctb_row, uint32_t ctb_col);
#endif
//} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
