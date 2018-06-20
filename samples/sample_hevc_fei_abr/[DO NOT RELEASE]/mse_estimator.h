/******************************************************************************\
Copyright (c) 2018, Intel Corporation
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

#ifndef __MSE_ESTIMATOR_
#define __MSE_ESTIMATOR_

#include "sample_hevc_fei_defs.h"
#include "bs_parser++.h"
#include <queue>
#include <map>

struct TransCoeffCuLevelStatData
{
    mfxU8  TrasformSize     = 0xff;
    bool   AllZeroQuant     = false;
    mfxU32 nSkip            = 0;
    mfxU32 nInter           = 0;
    mfxU32 nIntra           = 0;
    mfxU32 nZeroQuantCoeffs = 0;

    mfxU32 qpInterSum       = 0;
    mfxU32 numInterCu       = 0;

    mfxU32 qpIntraSum       = 0;
    mfxU32 numIntraCu       = 0;

    mfxU32 qpSkipSum        = 0;
    mfxU32 numSkipCu        = 0;

    mfxI64 SumOfCoeffs        = 0;
    mfxU64 SumOfSquaredCoeffs = 0.0;
    mfxF64 Variance           = 0.0;

    TransCoeffCuLevelStatData(mfxU8 level)
        : TrasformSize       (level)
        , AllZeroQuant       (false)
        , nSkip              (0)
        , nInter             (0)
        , nIntra             (0)
        , nZeroQuantCoeffs   (0)
        , qpInterSum         (0)
        , numInterCu         (0)
        , qpIntraSum         (0)
        , numIntraCu         (0)
        , qpSkipSum          (0)
        , SumOfCoeffs        (0)
        , SumOfSquaredCoeffs (0.0)
        , Variance           (0.0)
    {}
};

/* MseFrameStatData is a structure which holds all necessary statistics of frame for exponential regression
   which used to estimate distribution for all-zero-quantized transform levels
*/
struct EstimatorFrameStatData
{
    mfxI32 POC        = -100;
    mfxU8  qpOffset   = 0;
    mfxU32 nPixels    = 0;

    mfxU8  MinActiveCuLevel = 8;
    mfxU8  MaxActiveCuLevel = 32;

    TransCoeffCuLevelStatData Transform64x64 = TransCoeffCuLevelStatData(64);
    TransCoeffCuLevelStatData Transform32x32 = TransCoeffCuLevelStatData(32);
    TransCoeffCuLevelStatData Transform16x16 = TransCoeffCuLevelStatData(16);
    TransCoeffCuLevelStatData Transform8x8   = TransCoeffCuLevelStatData(8);

    mfxF64 EstimatedMSE = -1.0;

    std::map<mfxI32, mfxU32> POCtoNumSkipPixelsRef;

/* TMP CODE FOR TESTING */
    mfxU32 numNonZeroCoeff = 0;
    mfxU64 sumOfSquaredCoeff = 0;

    void Reset()
    {
        POC              = -100;
        qpOffset         = 0;
        nPixels          = 0;
        MinActiveCuLevel = 8;
        MaxActiveCuLevel = 32;

        Transform64x64 = TransCoeffCuLevelStatData(64);
        Transform32x32 = TransCoeffCuLevelStatData(32);
        Transform16x16 = TransCoeffCuLevelStatData(16);
        Transform8x8   = TransCoeffCuLevelStatData(8);

        EstimatedMSE   = -1.0;
        POCtoNumSkipPixelsRef.clear();

/* TMP CODE FOR TESTING */
        numNonZeroCoeff = 0;
        sumOfSquaredCoeff = 0;
    }
};

class MseEstimator
{
public:
    MseEstimator(MSE_EST_ALGO algo)
        : m_AlgorythmType(algo)
    {}

    void ProcessOneCU(const BS_HEVC2::CU* cu, const std::vector<mfxI32> refListActive[2]);
    void SetFrameParameters(const BS_HEVC2::SPS* sps);

    mfxF64 EstimateMSE();

    void StartNewFrame(std::vector<mfxI32>& dpb, mfxI32 poc)
    {
        // If it's not a first frame, put in frame cache
        if (m_CurrentFrameData.POC != -100)
        {
            m_FrameStatQueue.emplace_back(std::move(m_CurrentFrameData));

            if (m_FrameStatQueue.size() > m_FrameStatQueueSizeLimit)
            {
                m_FrameStatQueue.pop_front();
            }

            m_POCtoMSE[m_CurrentFrameData.POC] = m_CurrentFrameData.EstimatedMSE;

            // Remove from cache frames which have been removed from dpb
            for (auto it = m_POCtoMSE.begin(); it != m_POCtoMSE.end();)
            {
                if (auto it_find = std::find(dpb.begin(), dpb.end(), it->first) != dpb.end())
                {
                    ++it;
                }
                else
                {
                    it = m_POCtoMSE.erase(it);
                }
            }
        }

        // Clear data of previous frame
        m_CurrentFrameData.Reset();

        // Set POC for current frame
        m_CurrentFrameData.POC = poc;
    }

private:
/* TMP CODE FOR TESTING */
    MSE_EST_ALGO m_AlgorythmType = FAIR;

    EstimatorFrameStatData             m_CurrentFrameData;

    std::list<EstimatorFrameStatData>  m_FrameStatQueue;
    mfxU32                             m_FrameStatQueueSizeLimit = 3;

    std::map<mfxI32, mfxF64>           m_POCtoMSE;

    void AddToCurrentPOCmap(mfxI32 poc, mfxU32 numOfPixels)
    {
        if (m_CurrentFrameData.POCtoNumSkipPixelsRef.find(poc) != m_CurrentFrameData.POCtoNumSkipPixelsRef.end())
        {
            m_CurrentFrameData.POCtoNumSkipPixelsRef[poc] += numOfPixels;
        }
        else
        {
            m_CurrentFrameData.POCtoNumSkipPixelsRef[poc]  = numOfPixels;
        }
    }

    mfxF64 GetMeanVar(mfxU8 CuSize);

    mfxF64 EstimateGam(mfxF64 nu);
    mfxF64 EstimateNu();
};
#endif
