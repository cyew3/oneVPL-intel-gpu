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
    Ipp32s bitrate;
    Ipp64f framerate;
    Ipp8u  constQuant;
}VC1BRInfo;

class VC1BitRateControl
{
    public:
        VC1BitRateControl();
        ~VC1BitRateControl();

    protected:

        Ipp32u m_bitrate;           //current bitrate
        Ipp64f m_framerate;         //current frame rate

        //GOP BRC
        VC1BRC*                     m_BRC;

        //hypothetical ref decoder
        VC1HRDecoder** m_HRD;
        Ipp32s m_LBucketNum;

        //Frames parameters
        Ipp32s m_currSize;         //last coded frame size

        //GOP parameters
        Ipp32u m_GOPLength;         //number of frame in GOP
        Ipp32u m_BFrLength;         //number of successive B frames

         //coding params
         Ipp32u m_encMode;        //coding mode
         Ipp8u  m_lastQuant;      //last quant for skip frames

    public:
        static Ipp32s CalcAllocMemorySize(Ipp32u GOPLength, Ipp32u BFrLength);
        UMC::Status Init(Ipp8u* pBuffer,   Ipp32s AllocatedMemSize,
                         Ipp32u yuvFSize,  Ipp32u bitrate,
                         Ipp64f framerate, Ipp32u mode,
                         Ipp32u GOPLength, Ipp32u BFrLength, Ipp8u doubleQuant, Ipp8u QuantMode);

        UMC::Status InitBuffer(Ipp32s profile, Ipp32s level, Ipp32s BufferSize, Ipp32s initFull);

        void Reset();
        void Close();

        static Ipp8u GetLevel(Ipp32u profile, Ipp32u bitrate, Ipp32u widthMB, Ipp32u heightMB);

        //get and prepare data for coding (all data compressed as needed (see standard))
        void GetAllHRDParams(VC1_HRD_PARAMS* param);
        UMC::Status GetCurrentHRDParams(Ipp32s HRDNum, VC1_hrd_OutData* hrdParams);

        //return number of coeffs witch will be reduce by zero
        //1 - intra block, 0 - inter block
        //Ipp32s CheckBlockQuality(Ipp16s* pSrc, Ipp32u srcStep,
        //                         Ipp32u quant, Ipp32u intra);
        UMC::Status CheckFrameCompression(ePType picType, Ipp32u currSize);
        UMC::Status CompleteFrame(ePType picType);
        UMC::Status HandleHRDResult(Ipp32s hrdStatus);

        //return recomended quant
        void GetQuant(ePType picType, Ipp8u* PQuant, bool* Half);
        void GetQuant(ePType picType, Ipp8u* PQuant, bool* Half, bool* Uniform);

        UMC::Status GetBRInfo(VC1BRInfo* pInfo);

        UMC::Status ChangeGOPParams(Ipp32u GOPLength, Ipp32u BFrLength);

        Ipp8u GetLastQuant();

        Ipp32s GetLBucketNum();
protected:

        Ipp32s GetHRD_Rate  (Ipp32s* rate);
        Ipp32s GetHRD_Buffer(Ipp32s* size);

#ifdef BRC_TEST
        BRCTest  BRCFileSizeTest;
#endif
};

}

#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
