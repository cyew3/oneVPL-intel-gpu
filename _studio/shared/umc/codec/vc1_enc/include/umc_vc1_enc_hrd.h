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

#ifndef _ENCODER_VC1_HRD_H_
#define _ENCODER_VC1_HRD_H_


#include "ippdefs.h"
#include "umc_structures.h"
#include "umc_vc1_common_defs.h"

namespace UMC_VC1_ENCODER
{
//#define VC1_HRD_DEBUG

extern uint32_t bMax_LevelLimits[3][5];
extern uint32_t rMax_LevelLimits[3][5];
extern uint32_t MBfLimits[3][5];


enum
{
    VC1_HRD_OK           = 0x0,
    //VC1_HRD_OK_SMALL     = 0x1,
    //VC1_HRD_OK_BIG       = 0x2,
    //VC1_HRD_ERR_SMALL    = 0x4,
    //VC1_HRD_ERR_BIG      = 0x8,
    VC1_HRD_ERR_ENC_NOT_ENOUGH_DATA   = 0x10,
    VC1_HRD_ERR_ENC_NOT_ENOUGH_BUFFER = 0x20,
    VC1_HRD_ERR_DEC_NOT_ENOUGH_DATA   = 0x40,
    VC1_HRD_ERR_DEC_NOT_ENOUGH_BUFFER = 0x80,
    VC1_HRD_ERR_BIG_FRAME             = 0x100

};

typedef struct
{
    int32_t vc1_decFullness;
    int32_t vc1_decPrevFullness;
    int32_t vc1_decBufferSize;
    int32_t vc1_BitRate;
    double vc1_FrameRate;
    int32_t vc1_decInitFull;
    int32_t vc1_dec_delay;
    int32_t vc1_enc_delay;
    int32_t vc1_Prev_enc_delay;
}VC1_enc_LeakyBucketInfo;

typedef struct
{
    int32_t hrd_max_rate;                //maximum rate
    int32_t hrd_buffer;                  //buffer size
    int32_t hrd_fullness;                //hrd fullness
}VC1_hrd_OutData;

class VC1HRDecoder
{
    public:
        VC1HRDecoder();
        ~VC1HRDecoder();
        UMC::Status InitBuffer(int32_t profile,   int32_t level,
                               int32_t bufferSize,int32_t initFull,
                               int32_t bitRate,   double frameRate);

        void Reset();

        int32_t CheckBuffer(int32_t frameSize);
        int32_t ReleaseData(int32_t frameSize);


        void SetBitRate(int32_t bitRate);
        void SetFrameRate(int32_t frameRate);
        void GetHRDParams(VC1_hrd_OutData* hrdParams);
protected:
        int32_t AddNewData();
        int32_t RemoveFrame(int32_t frameSize);
        void   SetHRDRate(int32_t frameSize);
        void   SetHRDFullness();

    protected:
        VC1_enc_LeakyBucketInfo m_LBuckets;
        int32_t m_IdealFrameSize;            //in bits
        VC1_hrd_OutData hrd_data;
        //Flags
        int32_t m_recoding;         //frame was recoded


};
}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
