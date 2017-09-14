//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include "mfx_platform_headers.h"

#include <memory>
#include <map>

#if defined (MFX_ENABLE_VPP) && defined(MFX_VA_LINUX) && defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)

#ifndef __MFX_VPP_SCD_H
#define __MFX_VPP_SCD_H

// Include needed for CM wrapper
#include "cmrtex.h"
#include "cmrt_cross_platform.h"

#include <limits.h>
#ifdef MFX_VA_LINUX
  #include <immintrin.h> // SSE, AVX
#else
  #include <intrin.h>
#endif

// Pre-built SCD kernels
#include "asc_genx_bdw_isa.cpp"
#include "asc_genx_hsw_isa.cpp"
#include "asc_genx_skl_isa.cpp"


#define OUT_BLOCK         16
#define BLOCK_SIZE        4
#define NumTSC            10
#define NumSC             10
#define FLOAT_MAX         2241178.0
#define FRAMEMUL          16
#define CHROMASUBSAMPLE   4
#define SMALL_WIDTH       112
#define SMALL_HEIGHT      64
#define LN2               0.6931471805599453
#define MEMALLOCERROR     1000
#define MEMALLOCERRORU8   1001
#define MEMALLOCERRORMV   1002

#define NMAX(a,b)         ((a>b)?a:b)
#define NMIN(a,b)         ((a<b)?a:b)
#define NABS(a)           (((a)<0)?(-(a)):(a))
#define NAVG(a,b)         ((a+b)/2)

#define Clamp(x)          ((x<0)?0:((x>255)?255:x))
#define TSCSTATBUFFER     3

#define EXTRANEIGHBORS
#define SAD_SEARCH_VSTEP  2  // 1=FS 2=FHS
#define DECISION_THR      6 //Total number of trees is 13, decision has to be bigger than 6 to say it is a scene change.

//using namespace MfxHwVideoProcessing;

/* Special scene change detector classes/structures */

enum cpuOptimizations
{
    CPU_NONE = 0,
    CPU_SSE4,
    CPU_AVX2
};

enum Simil
{
    Not_same,
    Same
};

enum GoP_Sizes
{
    Forbidden_GoP,
    Immediate_GoP,
    QuarterHEVC_GoP,
    Three_GoP,
    HalfHEVC_GoP,
    Five_GoP,
    Six_GoP,
    Seven_Gop,
    HEVC_Gop,
    Nine_Gop,
    Ten_Gop,
    Eleven_Gop,
    Twelve_Gop,
    Thirteen_Gop,
    Fourteen_Gop,
    Fifteen_Gop,
    Double_HEVC_Gop
};

enum Data_Flow_Direction
{
    Reference_Frame,
    Current_Frame
};

enum FrameFields
{
    TopField,
    BottomField
};

enum FrameTypeScan
{
    progressive_frame,
    topfieldfirst_frame,
    bottomfieldFirst_frame
};

enum BufferPosition
{
    previous_previous_frame_data,
    previous_frame_data,
    current_frame_data
};

enum Layers
{
    Full_Size,
    Small_Size
};

typedef struct coordinates
{
    mfxI32 x;
    mfxI32 y;
} MVector;

typedef struct SAD_range
{
    mfxU32  SAD;
    mfxU32  distance;
    mfxU32  direction;
    MVector BestMV;
    mfxI32  angle;
} Rsad;

typedef struct YUV_layer_store {
   mfxU8 *data;
   mfxU8 *Y;
   mfxU8 *U;
   mfxU8 *V;
} YUV;

typedef struct Image_details {
    mfxI32 Original_Width;             //User input
    mfxI32 Original_Height;            //User input
    mfxI32 horizontal_pad;             //User input for original resolution in multiples of FRAMEMUL; derived for the other two layers
    mfxI32 vertical_pad;               //User input for original resolution in multiples of FRAMEMUL; derived for the other two layers
    mfxI32 _cwidth;                    //corrected size if width not multiple of FRAMEMUL
    mfxI32 _cheight;                   //corrected size if height not multiple of FRAMEMUL
    mfxI32 pitch;                      //horizontal_pad + _cwidth + horizontal_pad
    mfxI32 Extended_Width;             //horizontal_pad + _cwidth + horizontal_pad
    mfxI32 Extended_Height;            //vertical_pad + _cheight + vertical_pad
    mfxI32 Total_non_corrected_pixels;
    mfxI32 Pixels_in_Y_layer;          //_cwidth * _cheight
    mfxI32 Pixels_in_U_layer;          //Pixels_in_Y_layer / 4 (assuming 4:2:0)
    mfxI32 Pixels_in_V_layer;          //Pixels_in_Y_layer / 4 (assuming 4:2:0)
    mfxI32 Pixels_in_full_frame;       //Pixels_in_Y_layer * 3 / 2 (assuming 4:2:0)
    mfxI32 block_width;                //User input
    mfxI32 block_height;               //User input
    mfxI32 Pixels_in_block;            //block_width x block_height
    mfxI32 Width_in_blocks;            //_cwidth / block_width
    mfxI32 Height_in_blocks;           //_cheight / block_height
    mfxI32 Blocks_in_Frame;            //Pixels_in_Y_layer / Pixels_in_block
    mfxI32 initial_point;              //(Extended_Width * vertical_pad) + horizontal_pad
    mfxI32 sidesize;                   //_cheight + (1 * vertical_pad)
    mfxI32 endPoint;                   //(sidesize * Extended_Width) - horizontal_pad
    mfxI32 MVspaceSize;                //Pixels_in_Y_layer / block_width / block_height
} ImDetails;

typedef struct Video_characteristics {
    ImDetails *layer;
    char   *inputFile;
    mfxI32  starting_frame;              //Frame number where the video is going to be accessed
    mfxI32  total_number_of_frames;      //Total number of frames in video file
    mfxI32  processed_frames;            //Number of frames that are going to be processed; taking into account the starting frame
    mfxI32  accuracy;
    mfxI32  key_frame_frequency;
    mfxI32  limitRange;
    mfxI32  maxXrange;
    mfxI32  maxYrange;
    mfxI32  interlaceMode;
    mfxI32  StartingField;
    mfxI32  currentField;
}VidData;

typedef struct frameBuffer
{
    YUV      Image;
    MVector *pInteger;
    mfxF32   CsVal;
    mfxF32   RsVal;
    mfxF32   avgval;
    mfxU32   *SAD;
    mfxF32   *Cs;
    mfxF32   *Rs;
    mfxF32   *RsCs;
}imageData;

typedef struct stats_structure
{
    mfxI32 frameNum;
    mfxI32 SCindex;
    mfxI32 TSCindex;
    mfxI32 scVal;
    mfxI32 tscVal;
    mfxI32 pdist;
    mfxI32 histogram[5];
    mfxI32 Schg;
    mfxI32 last_shot_distance;
    mfxF32 Rs;
    mfxF32 Cs;
    mfxF32 SC;
    mfxF32 AFD;
    mfxF32 TSC;
    mfxF32 RsDiff;
    mfxF32 CsDiff;
    mfxF32 RsCsDiff;
    mfxF32 MVdiffVal;
    mfxF32 diffAFD;
    mfxF32 diffTSC;
    mfxF32 diffRsCsDiff;
    mfxF32 diffMVdiffVal;
    mfxF64 gchDC;
    mfxF64 posBalance;
    mfxF64 negBalance;
    mfxF64 ssDCval;
    mfxF64 refDCval;
    mfxF64 avgVal;
    mfxI64 ssDCint;
    mfxI64 refDCint;
    BOOL   Gchg;
    mfxU8  picType;
    mfxU8  lastFrameInShot;
    BOOL   repeatedFrame;
    BOOL   firstFrame;
} TSCstat;

typedef struct videoBuffer {
    imageData *layer;
    mfxI32     frame_number;
    mfxI32     forward_reference;
    mfxI32     backward_reference;
} VidSample;

typedef struct extended_storage {
    mfxF64      average;
    mfxF32      avgSAD;
    mfxU32      gopSize;
    mfxU32      lastSCdetectionDistance;
    mfxU32      detectedSch;
    mfxU32      pendingSch;
    TSCstat   **logic;
    // For Pdistance table selection
    mfxI32      *PDistanceTable;
    Layers      size;
    BOOL        firstFrame;
    imageData   gainCorrection;
} VidRead;

typedef mfxU8*             pmfxU8;
typedef mfxI8*             pmfxI8;
typedef mfxI16*            pmfxI16;
typedef mfxU16*            pmfxU16;
typedef mfxU32*            pmfxU32;
typedef mfxUL32*           pmfxUL32;
typedef mfxL32*            pmfxL32;
typedef mfxF32*            pmfxF32;
typedef mfxF64*            pmfxF64;
typedef mfxU64*            pmfxU64;
typedef mfxI64*            pmfxI64;

static mfxI32 PDISTTbl2mod[NumTSC*NumSC] =
{
    2, 3, 4, 5, 5, 5, 5, 5, 5, 5,
    2, 2, 3, 4, 4, 4, 5, 5, 5, 5,
    1, 2, 2, 3, 3, 3, 4, 4, 5, 5,
    1, 1, 2, 2, 3, 3, 3, 4, 4, 5,
    1, 1, 2, 2, 2, 2, 2, 3, 3, 4,
    1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
    1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 3,
    1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
static mfxI32 PDISTTbl2[NumTSC*NumSC] =
{
    2, 3, 3, 4, 4, 5, 5, 5, 5, 5,
    2, 2, 3, 3, 4, 4, 5, 5, 5, 5,
    1, 2, 2, 3, 3, 3, 4, 4, 5, 5,
    1, 1, 2, 2, 3, 3, 3, 4, 4, 5,
    1, 1, 2, 2, 3, 3, 3, 3, 3, 4,
    1, 1, 1, 2, 2, 3, 3, 3, 3, 3,
    1, 1, 1, 1, 2, 2, 3, 3, 3, 3,
    1, 1, 1, 1, 2, 2, 2, 3, 3, 3,
    1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
static mfxI32 PDISTTbl3[NumTSC*NumSC] =
{
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    4, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
    3, 3, 4, 4, 4, 5, 5, 5, 5, 5,
    3, 3, 3, 4, 4, 4, 4, 5, 5, 5,
    2, 2, 2, 2, 3, 4, 4, 4, 5, 5,
    1, 2, 2, 2, 2, 3, 4, 4, 4, 5,
    1, 1, 2, 2, 2, 2, 3, 3, 4, 4

};
static mfxF32 lmt_sc2[NumSC] = { 4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78.0, FLOAT_MAX }; // lower limit of SFM(Rs,Cs) range for spatial classification
// 9 ranges of SC are: 0 0-4, 1 4-9, 2 9-15, 3 15-23, 4 23-32, 5 32-44, 6 42-53, 7 53-65, 8 65-78, 9 78->??
static mfxF32 lmt_tsc2[NumTSC] = { 0.75, 1.5, 2.25, 3.0, 4.0, 5.0, 6.0, 7.5, 9.25, FLOAT_MAX };   // lower limit of AFD
// 8 ranges of TSC (based on FD) are:0 0-0.75 1 0.75-1.5, 2 1.5-2.25. 3 2.25-3, 4 3-4, 5 4-5, 6 5-6, 7 6-7.5, 8 7.5-9.25, 9 9.25->??
static mfxF32 TH[4] = { -12.0, -4.0, 4.0, 12.0 };

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
#define SCD_CPU_DISP(OPT,func, ...)  (( CPU_AVX2 == OPT ) ? func ## _AVX2(__VA_ARGS__) : (( CPU_SSE4 == OPT ) ? func ## _SSE4(__VA_ARGS__) : func ## _C(__VA_ARGS__) ))
#else
#define SCD_CPU_DISP(OPT,func, ...)  (( CPU_SSE4 == OPT ) ? func ## _SSE4(__VA_ARGS__) : func ## _C(__VA_ARGS__) )
#endif

class SceneChangeDetector
{
public:
    SceneChangeDetector()
        : m_mfxDeviceType()
        , m_mfxDeviceHdl(0)
        , m_pCmDevice(0)
        , gpustep_w(0)
        , gpustep_h(0)
        , m_gpuwidth(0)
        , m_gpuheight(0)
        , m_bInited(false)
        , m_cpuOpt(CPU_NONE)
        , support(0)
        , m_dataIn(0)
        , videoData(0)
        , dataReady(false)
        , GPUProc(false)
        , _width(0)
        , _height(0)
        , _pitch(0) 
        , m_pCore(NULL)
    {};

    ~SceneChangeDetector()
    {
       if (m_bInited)
       {
          Close();
       }
    }

    mfxStatus Init(VideoCORE   *core, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxU32 interlaceMode, mfxHandleType _mfxDeviceType, mfxHDL _mfxDeviceHdl);

    mfxStatus Close();

    mfxStatus SetGoPSize(mfxU32 GoPSize);

    mfxStatus MapFrame(mfxFrameSurface1 *pSurf);

    BOOL      ProcessField();
    mfxU32    Get_frame_number();
    mfxU32    Get_frame_shot_Decision();
    mfxU32    Get_frame_last_in_scene();
    BOOL      Query_is_frame_repeated();

    void      SetParityTFF();    //Sets the detection to interlaced Top Field First mode, can be done on the fly
    void      SetParityBFF();    //Sets the detection to interlaced Bottom Field First mode, can be done on the fly
    void      SetProgressiveOp();//Sets the detection to progressive frame mode, can be done on the fly

private:
    void   ShotDetect(imageData Data, imageData DataRef, ImDetails imageInfo, TSCstat *current, TSCstat *reference);
    void   SetInterlaceMode(mfxU32 interlaceMode);
    void   ResetGoPSize();
    void   PutFrameProgressive();
    void   PutFrameInterlaced();
    BOOL   PutFrame();
    BOOL   Get_Last_frame_Data();
    mfxU32 Get_starting_frame_number();
    BOOL   Get_GoPcorrected_frame_shot_Decision();
    mfxI32 Get_frame_Spatial_complexity();
    mfxI32 Get_frame_Temporal_complexity();
    mfxI32 Get_stream_parity();
    BOOL   SCDetectGPU(mfxF64 diffMVdiffVal, mfxF64 RsCsDiff,   mfxF64 MVDiff,   mfxF64 Rs,       mfxF64 AFD,
                                mfxF64 CsDiff,        mfxF64 diffTSC,    mfxF64 TSC,      mfxF64 gchDC,    mfxF64 diffRsCsdiff,
                                mfxF64 posBalance,    mfxF64 SC,         mfxF64 TSCindex, mfxF64 Scindex,  mfxF64 Cs,
                                mfxF64 diffAFD,       mfxF64 negBalance, mfxF64 ssDCval,  mfxF64 refDCval, mfxF64 RsDiff);

    void   VidSample_Alloc();
    void   VidSample_dispose();
    void   ImDetails_Init(ImDetails *Rdata);
    void   nullifier(imageData *Buffer);
    void   imageInit(YUV *buffer);
    void   VidRead_dispose();
    mfxStatus SetWidth(mfxI32 Width);
    mfxStatus SetHeight(mfxI32 Height);
    mfxStatus SetPitch(mfxI32 Pitch);
    void SetNextField();
    mfxStatus SetDimensions(mfxI32 Width, mfxI32 Height, mfxI32 Pitch);
    void GpuSubSampleASC_Image(mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, Layers dstIdx, mfxU32 parity);
    mfxStatus SubSampleImage(mfxU32 srcWidth, mfxU32 srcHeight, const unsigned char *pData);

    void ReadOutputImage();
    void GPUProcess();
    void alloc();
    void IO_Setup();
    void Setup_UFast_Environment();
    void SetUltraFastDetection();
    void Params_Init();
    void InitStruct();
    void VidRead_Init();
    void VidSample_Init();
    BOOL CompareStats(mfxU8 current, mfxU8 reference, BOOL isInterlaced);
    BOOL FrameRepeatCheck(BOOL isInterlaced);
    void processFrame();
    void GeneralBufferRotation();
    BOOL RunFrame(mfxU32 parity);

    void RsCsCalc(imageData *exBuffer, ImDetails vidCar);

    /* Motion estimation stuff */
    void MotionAnalysis(VidRead *support, VidData *dataIn, VidSample *videoIn, VidSample *videoRef, mfxF32 *TSC, mfxF32 *AFD, mfxF32 *MVdiffVal, Layers lyrIdx);
    mfxI32 __cdecl HME_Low8x8fast(VidRead *videoIn, mfxI32 fPos, ImDetails dataIn, imageData *scale, imageData *scaleRef, BOOL first, mfxU32 accuracy, VidData limits);

    mfxHandleType m_mfxDeviceType;
    mfxHDL        m_mfxDeviceHdl;

    CmDevice        *m_pCmDevice;
    CmProgram       *m_pCmProgram;
    CmKernel        *m_pCmKernel;
    CmQueue         *m_pCmQueue;
    CmBufferUP      *m_pCmBufferOut;
    CmThreadSpace   * m_pCmThreadSpace;

    std::map<void *, CmSurface2D *> m_tableCmRelations;

    static const int subWidth = 112; // width of SCD output image
    static const int subHeight = 64; // heigh of SCD output image

    int gpustep_w;
    int gpustep_h;
    int m_gpuwidth;
    int m_gpuheight;

    BOOL         m_bInited; // Indicates if SCD inited or not
    mfxU16       m_cpuOpt; // CPU optimizations available
    VidRead     *support;
    VidData     *m_dataIn;
    VidSample   **videoData; // contains image data and statistics. videoData[2] only has video data
    BOOL         dataReady;
    BOOL         GPUProc;
    mfxI32      _width, _height, _pitch;
    VideoCORE   *m_pCore;
};

#endif //__MFX_VPP_SCD_H
#endif //defined (MFX_ENABLE_VPP) && defined(MFX_VA_LINUX)
