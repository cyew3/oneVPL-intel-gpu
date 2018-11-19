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

#ifndef _ENCODER_VC1_BITRATE_GOP_H_
#define _ENCODER_VC1_BITRATE_GOP_H_

#include "ippdefs.h"
#include "umc_defs.h"
#include "umc_structures.h"
#include "umc_vc1_enc_def.h"

namespace UMC_VC1_ENCODER
{
#ifdef VC1_ENC_DEBUG_ON
    //#define VC1_BRC_DEBUG
    //#define VC1_GOP_DEBUG
#endif

//bitrate mode
enum
{
    //could be a lot of recodingfor the best
    //possible for current bitrate quality
    VC1_BRC_HIGHT_QUALITY_MODE = 0x0,

    //quant coeefs will not changed
    VC1_BRC_CONST_QUANT_MODE = 0x1,
};

#define VC1_RATIO_ILIM     1.9
#define VC1_RATIO_PLIM     1.5
#define VC1_RATIO_BLIM     1.45

#define VC1_MIN_RATIO_HALF     0.8

#define VC1_RATIO_IMIN     0.4
#define VC1_RATIO_PMIN     0.4
#define VC1_RATIO_BMIN     1.4

#define VC1_GOOD_COMPRESSION 0.90
#define VC1_POOR_REF_FRAME   0.75

#define VC1_P_SIZE         0.8
#define VC1_B_SIZE         0.6

typedef struct
{
    uint8_t IQuant;
    uint8_t PQuant;
    uint8_t BQuant;

    bool   IHalf;
    bool   PHalf;
    bool   BHalf;

    uint8_t LimIQuant;
    uint8_t LimPQuant;
    uint8_t LimBQuant;
}VC1PicQuant;

typedef struct
{
    uint8_t PQIndex;
    bool   HalfPQ;
}VC1Quant;

#define VC1_MIN_QUANT      1
#define VC1_MAX_QUANT      31
#define VC1_QUANT_CLIP(quant, lim_quant)                \
        {                                               \
          if ((quant) < (VC1_MIN_QUANT))                \
              (quant) = (VC1_MIN_QUANT);                \
          else if ((quant) > (VC1_MAX_QUANT))           \
              (quant) = (VC1_MAX_QUANT);                \
          if((lim_quant > 0) && (quant < lim_quant))    \
            quant = lim_quant;                          \
        }

#define VC1_CHECK_QUANT(quant)                          \
        {                                               \
          if ((quant) < (VC1_MIN_QUANT))                \
              (quant) = (VC1_MIN_QUANT);                \
          else if ((quant) > (VC1_MAX_QUANT))           \
              (quant) = (VC1_MAX_QUANT);                \
        }

enum
{
    VC1_BRC_OK                      = 0x0,
    VC1_BRC_ERR_BIG_FRAME           = 0x1,
    VC1_BRC_BIG_FRAME               = 0x2,
    VC1_BRC_ERR_SMALL_FRAME         = 0x4,
    VC1_BRC_SMALL_FRAME             = 0x8,
    VC1_BRC_NOT_ENOUGH_BUFFER       = 0x10
};

class VC1BRC
{
public:
    VC1BRC();
    virtual ~VC1BRC();

    virtual UMC::Status Init(uint32_t /*yuvFSize*/,  uint32_t /*bitrate*/, double /*framerate*/, uint32_t /*mode*/,
        uint32_t /*GOPLength*/, uint32_t /*BFrLength*/, uint8_t /*doubleQuant*/, uint8_t /*QuantMode*/){return UMC::UMC_ERR_NOT_IMPLEMENTED;};

    virtual void Reset(){};
    virtual void Close(){};

    //return frame compression staus
    virtual UMC::Status CheckFrameCompression(ePType /*picType*/, uint32_t /*currSize*/, UMC::Status /*HRDSts*/){return UMC::UMC_ERR_NOT_IMPLEMENTED;};
    virtual void GetQuant(ePType /*picType*/, uint8_t* /*PQuant*/, bool* /*Half*/){};
    virtual void GetQuant(ePType /*picType*/, uint8_t* /*PQuant*/, bool* /*Half*/, bool* /*Uniform*/){};

    virtual void CompleteFrame(ePType /*picType*/){};
    virtual UMC::Status SetGOPParams(uint32_t /*GOPLength*/, uint32_t /*BFrLength*/){return UMC::UMC_ERR_NOT_IMPLEMENTED;};

protected:
    uint32_t m_bitrate;           //current bitrate
    double m_framerate;         //current frame rate

    //GOP parameters
    uint32_t m_GOPLength;         //number of frame in GOP
    uint32_t m_CurrGOPLength;      //current distance between I frames

    uint32_t m_BFrLength;         //number of successive B frames
    int32_t m_SizeAbberation;    //coded size abberation

    //Flags
    int32_t m_recoding;         //frame was recoded

    //Quant params
    VC1Quant m_CurQuant;        //current quntization MQUANT and HALFQP
    VC1PicQuant m_Quant;        //current quant set for all picture

    //Frames parameters
    int32_t m_currSize;         //last coded frame size
    int32_t m_IdealFrameSize;    //"ideal" frame size

    //Picture params
    ePType m_picType;         //current picture type

    //coding params
    double m_ratio_min;      //bottom coded frame size ratio
    double m_buffer_overflow;//top coded frame size ratio
    uint32_t m_encMode;        //coding mode
    uint8_t  m_QuantMode;      //two bits: 1 bit - allow uniform mode coding, 2 bit - allow nonuniform mode
};

////////////////////////////////////////////
//          VC1BRC_I
////////////////////////////////////////////

class VC1BRC_I : public VC1BRC
{
public:
    VC1BRC_I();
    ~VC1BRC_I();
protected:
    int32_t m_needISize;        //wishful coded I frame size
public:
    UMC::Status Init(uint32_t yuvFSize,  uint32_t bitrate,
                         double framerate, uint32_t mode,
                         uint32_t GOPLength, uint32_t BFrLength, uint8_t doubleQuant, uint8_t QuantMode);

    void Reset();
    void Close();
    //return frame compression staus
    UMC::Status CheckFrameCompression(ePType picType, uint32_t currSize, UMC::Status HRDSts);
    void CompleteFrame(ePType picType);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half, bool* Uniform);
    UMC::Status SetGOPParams(uint32_t /*GOPLength*/, uint32_t /*BFrLength*/);

protected:
    void CheckFrame_QualityMode    (float ratio, UMC::Status HRDSts);
    void CorrectGOPQuant(float ratio);
    int32_t m_prefINeedSize;

};

////////////////////////////////////////////
//          VC1BRC_IP
////////////////////////////////////////////
class VC1BRC_IP : public VC1BRC
{
public:
    VC1BRC_IP();
    ~VC1BRC_IP();
protected:
    int32_t m_I_GOPSize;         //I frame size in current GOP
    int32_t m_P_GOPSize;         //P frames size in current GOP

    uint8_t  m_failPQuant;        //need when P ratio >> 1.5
    int32_t m_failGOP;        //need when P rati0 >> 1.5

    int32_t m_needISize;        //wishful coded I frame size
    int32_t m_needPSize;        //wishful coded P frame size

    int32_t m_INum;              //number I frame in GOP
    int32_t m_PNum;              //number P frame in GOP

    int32_t m_CurrINum;          //number coded I frame in current GOP
    int32_t m_CurrPNum;          //number coded P frame in current GOP
    uint32_t m_currFrameInGOP;    //numder of coded frames in current GOP
    int32_t m_poorRefFrame;     //last ref frame had poor quality
    int32_t m_GOPHalfFlag;      //less than half of GOP were coded

    //Frames parameters
    int32_t m_currISize;         //last coded I frame size
    int32_t m_currPSize;         //last coded P frame size
    int32_t m_GOPSize;           //"ideal" GOP size
    int32_t m_currGOPSize;       //current GOP size
    int32_t m_nextGOPSize;       //plan GOP size

    float m_IP_size;   //I/P frame size ratio
    int32_t m_AveragePQuant;
    int32_t m_AverageIQuant;
public:
    UMC::Status Init(uint32_t yuvFSize,  uint32_t bitrate,
                         double framerate, uint32_t mode,
                         uint32_t GOPLength, uint32_t BFrLength, uint8_t doubleQuant, uint8_t QuantMode);
    void Reset();
    void Close();

    //return frame compression staus
    UMC::Status CheckFrameCompression(ePType picType, uint32_t currSize, UMC::Status HRDSts);
    void CompleteFrame(ePType picType);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half, bool* Uniform);
    UMC::Status SetGOPParams(uint32_t GOPLength, uint32_t /*BFrLength*/);

protected:
    void CheckFrame_QualityMode    (ePType picType, float ratio, UMC::Status HRDSts);
    void CorrectGOPQuant(ePType picType, float ratio);

};

////////////////////////////////////////////
//          VC1BRC_IPB
////////////////////////////////////////////
class VC1BRC_IPB :  public VC1BRC
{
public:
    VC1BRC_IPB();
    ~VC1BRC_IPB();
protected:
    int32_t m_I_GOPSize;         //I frame size in current GOP
    int32_t m_P_GOPSize;         //P frames size in current GOP
    int32_t m_B_GOPSize;         //B frames size in current GOP


    uint8_t  m_failPQuant;        //need when P ratio >> 1.5
    uint8_t  m_failBQuant;        //need when B ratio >> 1.5

    int32_t m_failGOP;        //need when P rati0 >> 1.5
    int32_t m_needISize;        //wishful coded I frame size
    int32_t m_needPSize;        //wishful coded I frame size
    int32_t m_needBSize;        //wishful coded I frame size

    int32_t m_INum;              //number I frame in GOP
    int32_t m_PNum;              //number P frame in GOP
    int32_t m_BNum;              //number B frame in GOP

    int32_t m_CurrINum;          //number coded I frame in current GOP
    int32_t m_CurrPNum;          //number coded P frame in current GOP
    int32_t m_CurrBNum;          //number coded B frame in current GOP

    uint32_t m_currFrameInGOP;    //numder of coded frames in current GOP
    int32_t m_frameCount;        //numder of coded frames with current GOP params

    int32_t m_poorRefFrame;      //last ref frame had poor quality
    int32_t m_GOPHalfFlag;       //less than half of GOP were coded

    //Frames parameters
    int32_t m_currISize;         //last coded I frame size
    int32_t m_currPSize;         //last coded P frame size
    int32_t m_currBSize;         //last coded B frame size
    int32_t m_GOPSize;           //"ideal" GOP size
    int32_t m_currGOPSize;       //current GOP size
    int32_t m_nextGOPSize;       //plan GOP size
    float m_IP_size;           //I/P frame size ratio
    float m_IB_size;           //I/b frame size ratio

    int32_t m_AveragePQuant;
    int32_t m_AverageIQuant;
    int32_t m_AverageBQuant;

public:
    UMC::Status Init(uint32_t yuvFSize,  uint32_t bitrate,
                         double framerate, uint32_t mode,
                         uint32_t GOPLength, uint32_t BFrLength, uint8_t doubleQuant, uint8_t QuantMode);
    void Reset();
    void Close();

    //return frame compression staus
    UMC::Status CheckFrameCompression(ePType picType, uint32_t currSize, UMC::Status HRDSts);
    void CompleteFrame(ePType picType);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half);
    void GetQuant(ePType picType, uint8_t* PQuant, bool* Half, bool* Uniform);
    UMC::Status SetGOPParams(uint32_t GOPLength, uint32_t BFrLength);

protected:
    void CheckFrame_QualityMode    (ePType picType, float ratio, UMC::Status HRDSts);
    void CorrectGOPQuant(ePType picType, float ratio);
};
}
#endif //_ENCODER_VC1_BITRATE_GOP_H_
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
