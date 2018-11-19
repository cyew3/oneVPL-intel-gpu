// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_BITRATE_H_
#define _ENCODER_VC1_BITRATE_H_

#include "ippdefs.h"
#include "umc_vc1_enc_hrd.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_brc_gop.h"

namespace UMC_VC1_ENCODER
{

#define VC1_ENC_MAX_NUM_LEAKY_BUCKET 31 //0-31
#define VC1_ENC_LEAKY_BUCKET_NUM     1


#ifdef VC1_ENC_DEBUG_ON
  //#define BRC_TEST
#endif

#ifdef BRC_TEST
typedef struct
{
    FILE* RES_File;
} BRCTest;
#endif

typedef struct
{
    int32_t bitrate;
    double framerate;
    uint8_t  constQuant;
}VC1BRInfo;

class VC1BitRateControl
{
    public:
        VC1BitRateControl();
        ~VC1BitRateControl();

    protected:

        uint32_t m_bitrate;           //current bitrate
        double m_framerate;         //current frame rate

        //GOP BRC
        VC1BRC*                     m_BRC;

        //hypothetical ref decoder
        VC1HRDecoder** m_HRD;
        int32_t m_LBucketNum;

        //Frames parameters
        int32_t m_currSize;         //last coded frame size

        //GOP parameters
        uint32_t m_GOPLength;         //number of frame in GOP
        uint32_t m_BFrLength;         //number of successive B frames

         //coding params
         uint32_t m_encMode;        //coding mode
         uint8_t  m_lastQuant;      //last quant for skip frames

    public:
        static int32_t CalcAllocMemorySize(uint32_t GOPLength, uint32_t BFrLength);
        UMC::Status Init(uint8_t* pBuffer,   int32_t AllocatedMemSize,
                         uint32_t yuvFSize,  uint32_t bitrate,
                         double framerate, uint32_t mode,
                         uint32_t GOPLength, uint32_t BFrLength, uint8_t doubleQuant, uint8_t QuantMode);

        UMC::Status InitBuffer(int32_t profile, int32_t level, int32_t BufferSize, int32_t initFull);

        void Reset();
        void Close();

        static uint8_t GetLevel(uint32_t profile, uint32_t bitrate, uint32_t widthMB, uint32_t heightMB);

        //get and prepare data for coding (all data compressed as needed (see standard))
        void GetAllHRDParams(VC1_HRD_PARAMS* param);
        UMC::Status GetCurrentHRDParams(int32_t HRDNum, VC1_hrd_OutData* hrdParams);

        //return number of coeffs witch will be reduce by zero
        //1 - intra block, 0 - inter block
        //int32_t CheckBlockQuality(int16_t* pSrc, uint32_t srcStep,
        //                         uint32_t quant, uint32_t intra);
        UMC::Status CheckFrameCompression(ePType picType, uint32_t currSize);
        UMC::Status CompleteFrame(ePType picType);
        UMC::Status HandleHRDResult(int32_t hrdStatus);

        //return recomended quant
        void GetQuant(ePType picType, uint8_t* PQuant, bool* Half);
        void GetQuant(ePType picType, uint8_t* PQuant, bool* Half, bool* Uniform);

        UMC::Status GetBRInfo(VC1BRInfo* pInfo);

        UMC::Status ChangeGOPParams(uint32_t GOPLength, uint32_t BFrLength);

        uint8_t GetLastQuant();

        int32_t GetLBucketNum();
protected:

        int32_t GetHRD_Rate  (int32_t* rate);
        int32_t GetHRD_Buffer(int32_t* size);

#ifdef BRC_TEST
        BRCTest  BRCFileSizeTest;
#endif
};

}

#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
