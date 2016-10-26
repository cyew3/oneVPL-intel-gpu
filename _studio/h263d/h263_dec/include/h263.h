//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#ifndef _H263_H_
#define _H263_H_

#ifdef _OPENMP
#define _OMP_KARABAS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vm_debug.h"
#include "ippdefs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippi.h"
#include "ippvc.h"
#include "vm_debug.h"
#include "vm_thread.h"
//#include "ippcc.h"

#pragma warning(disable : 4710) // function not inlined
#pragma warning(disable : 4514) // unreferenced inline function has been removed CL
#pragma warning(disable : 4100) // unreferenced formal parameter CL

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
    #define __INLINE static __inline
#elif defined( __GNUC__ )
    #define __INLINE static __inline__
#else
    #define __INLINE static
#endif

#if defined(__INTEL_COMPILER) && !defined(_WIN32_WCE)
    #define __ALIGN16(type, name, size) \
        __declspec (align(16)) type name[size]
#else
    #if defined(_WIN64) || defined(WIN64) || defined(LINUX64)
        #define __ALIGN16(type, name, size) \
            Ipp8u _a16_##name[(size)*sizeof(type)+15]; type *name = (type*)(((Ipp64s)(_a16_##name) + 15) & ~15)
    #else
        #define __ALIGN16(type, name, size) \
            Ipp8u _a16_##name[(size)*sizeof(type)+15]; type *name = (type*)(((Ipp32s)(_a16_##name) + 15) & ~15)
    #endif
#endif

#define h263_CLIP(x, min, max) if ((x) < (min)) (x) = (min); else if ((x) > (max)) (x) = (max)
#define h263_CLIPR(x, max) if ((x) > (max)) (x) = (max)
#define h263_SWAP(type, x, y) {type t = (x); (x) = (y); (y) = t;}
#define h263_ABS(a) ((a) >= 0 ? (a) : -(a))

/* Timer Info */

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)

    #include <windows.h>

    typedef struct _h263_Timer {
        LARGE_INTEGER  count;
        LARGE_INTEGER  start;
        LARGE_INTEGER  stop;
        Ipp32s         calls;
    } h263_Timer;

    __INLINE void h263_TimerStart(h263_Timer *t)
    {
        QueryPerformanceCounter(&t->start);
    }

    __INLINE void h263_TimerStop(h263_Timer *t)
    {
        QueryPerformanceCounter(&t->stop);
        t->count.QuadPart += t->stop.QuadPart - t->start.QuadPart;
        t->calls ++;
    }

    #define TIMER_FREQ_TYPE LARGE_INTEGER

    __INLINE void h263_GetTimerFreq(TIMER_FREQ_TYPE *f)
    {
        QueryPerformanceFrequency(f);
    }

    __INLINE Ipp64f h263_GetTimerSec(h263_Timer *t, TIMER_FREQ_TYPE f)
    {
        return (Ipp64f)t->count.QuadPart / (Ipp64f)f.QuadPart;
    }

#else  // LINUX

    #include <time.h>

    typedef struct _h263_Timer {
        clock_t  count;
        clock_t  start;
        clock_t  stop;
        Ipp32s   calls;
    } h263_Timer;

    __INLINE void h263_TimerStart(h263_Timer *t)
    {
        t->start = clock();
    }

    __INLINE void h263_TimerStop(h263_Timer *t)
    {
        t->stop = clock();
        t->count += t->stop - t->start;
        t->calls ++;
    }

    #define TIMER_FREQ_TYPE Ipp32s

    __INLINE void h263_GetTimerFreq(TIMER_FREQ_TYPE *f)
    {
        *f = CLOCKS_PER_SEC;
    }

    __INLINE Ipp64f h263_GetTimerSec(h263_Timer *t, TIMER_FREQ_TYPE f)
    {
        return (Ipp64f)t->count / (Ipp64f)f;
    }
#endif

#define H263_PIC_TIME_INCR_RESOLUTION 1800000
#define H263_DEFAULT_FRAME_INTERVAL 60*1001

/* number of exterior MB */
#define H263_NUM_EXT_MB 1

#define H263_MAX_ENH_LAYERS 14

/* Statistic Info */
typedef struct _h263_Statistic {
    // Sequence Info
    Ipp32s      nPic;
    Ipp32s      nPic_I;
    Ipp32s      nPic_P;
    Ipp32s      nPic_PB;
    Ipp32s      nPic_B;
    Ipp32s      nPic_EI;
    Ipp32s      nPic_EP;
#ifdef H263_FULL_STAT
    Ipp32s      nMB;
    Ipp32s      nMB_INTER;
    Ipp32s      nMB_INTER_Q;
    Ipp32s      nMB_INTRA;
    Ipp32s      nMB_INTRA_Q;
    Ipp32s      nMB_INTER4V;
    Ipp32s      nMB_INTER4V_Q;
    Ipp32s      nMB_INTERPOLATE_PB;
    Ipp32s      nMB_INTERPOLATE_iPB;
    Ipp32s      nMB_BACKWARD_iPB;
    Ipp32s      nMB_FORWARD_iPB;
    Ipp32s      nMB_NOTCODED;
    Ipp32s      nMB_DIRECT_B;
    Ipp32s      nMB_INTERPOLATE_B;
    Ipp32s      nMB_BACKWARD_B;
    Ipp32s      nMB_FORWARD_B;
    Ipp32s      nB_INTRA_DC;
    Ipp32s      nB_INTRA_AC;
    Ipp32s      nB_INTER_C;
    Ipp32s      nB_INTER_NC;
#endif
    // app Timing Info
    h263_Timer  time_DecodeShow;    // decode + draw + file reading
    h263_Timer  time_Decode;        // decode + file reading
    h263_Timer  time_DecodeOnly;    // decode only
} h263_Statistic;

__INLINE void h263_StatisticInc(Ipp32s *s)
{
    *s = (*s) + 1;
}

// when using Full Statistic, FPS is less
#ifdef H263_FULL_STAT
#define h263_StatisticInc_(s) h263_StatisticInc(s)
#define h263_TimerStart_(t) h263_TimerStart(t)
#define h263_TimerStop_(t) h263_TimerStop(t)
#else
#define h263_StatisticInc_(s)
#define h263_TimerStart_(t)
#define h263_TimerStop_(t)
#endif

/* status codes */
typedef enum {
    H263_STATUS_OK           =  0,   // no error
    H263_STATUS_NO_MEM       = -1,   // out of memory
    H263_STATUS_FILE_ERROR   = -2,   // file error
    H263_STATUS_NOTSUPPORT   = -3,   // not supported mode
    H263_STATUS_PARSE_ERROR  = -4,   // failure in H.263++ stream parsing
    H263_STATUS_ERROR        = -5    // unknown/unspecified error
} h263_Status;

/* H.263++ code values */
/* ITU-T Recommendation H.263, Table 5 */
enum {
    H263_ASPECT_RATIO_FORBIDDEN  = 0,
    H263_ASPECT_RATIO_1_1        = 1,
    H263_ASPECT_RATIO_12_11      = 2,
    H263_ASPECT_RATIO_10_11      = 3,
    H263_ASPECT_RATIO_16_11      = 4,
    H263_ASPECT_RATIO_40_33      = 5,
    H263_ASPECT_RATIO_EXTPAR     = 15
};

enum {
    H263_PIC_TYPE_I    = 0,
    H263_PIC_TYPE_P    = 1,
    H263_PIC_TYPE_iPB  = 2,
    H263_PIC_TYPE_B    = 3,
    H263_PIC_TYPE_EI   = 4,
    H263_PIC_TYPE_EP   = 5,
    H263_PIC_TYPE_PB   = 6,
    H263_PIC_TYPE_P_UNREF = 7 // Sorenson 263
};

enum {
  H263_SCALABILITY_NONE       = 0,
  H263_SCALABILITY_TEMPORAL   = 1,
  H263_SCALABILITY_SNR        = 2,
  H263_SCALABILITY_SPATIAL_X  = 3,
  H263_SCALABILITY_SPATIAL_Y  = 4,
  H263_SCALABILITY_SPATIAL_XY = 5
};

#define H263_HEADER_MARKER  0x145  /* 1010 0010 1   */
#define H263_MV_MARKER      0x1    /* 0000 0000 01  */

/* Frame Info */
typedef struct _h263_Frame {
    Ipp8u*      apY;        // allocated with border
    Ipp8u*      apCb;       // allocated with border
    Ipp8u*      apCr;       // allocated with border
    Ipp32s      stepY;
    Ipp32s      stepCr;
    Ipp32s      stepCb;
    Ipp8u*      pY;         // real pointer
    Ipp8u*      pCb;        // real pointer
    Ipp8u*      pCr;        // real pointer
    Ipp32s      type;
    Ipp64s      time;
/* used for scalability, resize, can be calculated from width/height */
    Ipp32s      mbPerRow;
    Ipp32s      mbPerCol;
/* used for resampling */
    Ipp32s      width;
    Ipp32s      height;
    size_t      mid;
} h263_Frame;

/* Block Info for Intra Prediction */
typedef struct _h263_IntraPredBlock {
    struct _h263_IntraPredBlock  *predA;
    struct _h263_IntraPredBlock  *predC;
    Ipp16s      dct_acA[8];
    Ipp16s      dct_acC[8];
    Ipp16s      dct_dc;
} h263_IntraPredBlock;

/* Buffer for Intra Prediction */
typedef struct _h263_IntraPredBuff {
    h263_IntraPredBlock  *block;
} h263_IntraPredBuff;

/* MacroBlock Info for Motion */
typedef struct _h263_MacroBlock {
    IppMotionVector mv[4];
    Ipp8u           type; /* OBMC */
    Ipp8u           quant;
} h263_MacroBlock;

typedef struct _h263_PTYPEmodes {
    Ipp32s      UMV;
    Ipp32s      SAC;
    Ipp32s      advPred;
    Ipp32s      PBframes;
    Ipp32s      resample;
    Ipp32s      redResUp;
} h263_PTYPEmodes;

typedef struct _h263_OPPTYPEmodes {
  Ipp8u  UMV;
  Ipp8u  SAC;
  Ipp8u  advPred;
  Ipp8u  advIntra;
  Ipp8u  deblockFilt;
  Ipp8u  sliceStruct;
  Ipp8u  RPS;
  Ipp8u  ISD;
  Ipp8u  altInterVLC;
  Ipp8u  modQuant;
} h263_OPPTYPEmodes;

typedef struct _h263_PictureFormat {
  Ipp32s  PAR_code;
  Ipp32s  PAR_width;
  Ipp32s  PAR_height;
  Ipp32s  width;
  Ipp32s  height;
  Ipp32s  scalability_type;
} h263_PictureFormat;

/* Video Object Plane with Ipp16s header Info */
typedef struct _h263_VideoPictureH263 {
    Ipp32s      temporal_reference;
    Ipp32s      split_screen_indicator;
    Ipp32s      document_camera_indicator;
    Ipp32s      full_picture_freeze_release;
    Ipp32s      picture_coding_type;
    Ipp32s      plusptype;

    Ipp32s      prev_temporal_reference;

    h263_PTYPEmodes    modes;
    h263_OPPTYPEmodes  oppmodes;
    h263_PictureFormat format;
    Ipp32s      width;
    Ipp32s      height;
    Ipp32s      MacroBlockPerRow;
    Ipp32s      MacroBlockPerCol;

    /* plusptype fields  */
    Ipp32s      ufep;
    Ipp32s      PCF;
    Ipp32s      prev_PCF;
    Ipp32s      rtype;

    Ipp32s      CPM;
    Ipp32s      PSBI;
    Ipp32s      PAR_code;
    Ipp32s      PAR_width;
    Ipp32s      PAR_height;

    Ipp32s      pic_quant;
    Ipp32s      gob_number;
    Ipp32s      num_gobs_in_pic;
    Ipp32s      RowsPerGOB;
    Ipp32s      quant_scale;

    Ipp32s      GSBI;
    Ipp32s      GFID;

    Ipp32s      clock_conversion_code;
    Ipp32s      clock_divisor;
    Ipp32s      prev_clock_conversion_code;
    Ipp32s      prev_clock_divisor;
    Ipp32s      ext_temporal_reference;
    Ipp32s      unlimited_UMV;
    Ipp32s      sliceSubmodes; /* bit 1 - Rectangular slices; bit 2 - Arbitrary slice ordering */

    Ipp32s      enh_layer_num;
    Ipp32s      ref_layer_num;
    Ipp32s      RPSflags;

    Ipp32s      pred_temp_ref;

    Ipp32s      temporal_reference_B;
    Ipp32s      dbquant;

    Ipp32s      implicit_resample;
    Ipp32s      wda;
    IppMotionVector warpParams[4];
    Ipp32s      fillMode;
    Ipp32s      fillColor[3];

#ifndef UMC_H263_DISABLE_SORENSON
    Ipp32s      sorenson_format;
#endif
} h263_VideoPicture;

typedef struct _h263_EnhancedLayer {
  h263_Frame    c_Frame;
  h263_Frame    r_Frame;
  h263_Frame    n_Frame;
  h263_Frame    *l_Frame;  /* reference frame from the lower reference layer */
  h263_Frame    *v_Frame;  /* frame for output */
  h263_Frame    a_Frame;   /* aux frame used in resampling */
  IppMotionVector warpParams[4];
  Ipp32s        picIndex;
} h263_EnhancedLayer;

/* Video Sequence Info */
typedef struct _h263_VideoSequence {
    h263_Frame              cFrame;       /* current */
    h263_Frame              rFrame;       /* reference in past */
    h263_Frame              nFrame;       /* B-parts of PB-frames / B-frames */
    h263_Frame              bFrame;       /* B-frames */
    h263_Frame              aFrame;       /* aux frame used in resampling */
    h263_Frame              *vFrame;      /* frame for display */
    h263_Frame              *refFrame;    /* ptr to reference frame, used in resampling */
    h263_VideoPicture       VideoPicture;
    h263_OPPTYPEmodes       oppmodes[H263_MAX_ENH_LAYERS + 1];  /* layers 1(base)-15 */
    h263_PictureFormat      format[H263_MAX_ENH_LAYERS + 1];
    Ipp32s                  prev_pic_type[H263_MAX_ENH_LAYERS + 1];
    Ipp32s                  ref_layer_num[H263_MAX_ENH_LAYERS + 1] ;
    Ipp32s                  ref_layer_num_b[H263_MAX_ENH_LAYERS + 1];
    h263_EnhancedLayer      *enhLayers[H263_MAX_ENH_LAYERS];

    h263_MacroBlock         *MBinfo;
    Ipp32s                  num_of_MBs;
    Ipp32s                  frameCount;
    Ipp32s                  frameInterval;
    Ipp32s                  frameScale;
    Ipp32s                  picture_time_increment_resolution;
    Ipp32s                  PicIndex;
    h263_IntraPredBuff      IntraPredBuff;
    Ipp8u                   GOBboundary[72]; /* max mbPerCol */
    IppMotionVector         *Bmv; /* motion vector predictors (forw and back) in B-frames */

    IppMotionVector         warpParams[4];
    Ipp32s                  fillMode;
    Ipp32s                  fillColor[3];


    /*  h263_DataPartMacroBlock*     DataPartBuff; */
    Ipp32s                  Ppics_to_show;
    Ipp32s                  Bpics_to_show;
    /* for B-Frame direct mode */
    Ipp32s                   TRB, TRD;
    /* time increment of past and future frame for B-frame */
    Ipp64s                  rTime, nTime;
    /* Picture global time */
    Ipp64s                  pic_time, pic_time_pb;
    Ipp64s                  ref_pic_time, prevP_pic_time;
    Ipp32s                  ref_temporal_reference, prevP_temporal_reference;

    h263_Statistic          Statistic;
} h263_VideoSequence;

#ifdef _OMP_KARABAS
/* MacroBlock Info for MT */
typedef struct _h263_MacroBlockMT {
    Ipp16s    dctCoeffs[64*6];
    Ipp16s    dctCoeffs_B[64*6];
    IppMotionVector mvF[4];     // PB-Frame
    IppMotionVector mvB[4];     // PB-Frame
    Ipp8u     lnz[12];
    Ipp8u     cbpy;
    Ipp8u     cbpc;
    Ipp8u     bmb_type;         // PB-Frame
    Ipp8u     cbpb;             // PB-Frame
} h263_MacroBlockMT;
#endif // _OMP_KARABAS

/* Full Info */
typedef struct _h263_Info {
    Ipp8u*      buffer;         /* buffer header for saving H.263++ stream */
    size_t      buflen;         /* total buffer length */
    Ipp8u*      bufptr;         /* current frame, point to header or data */
    Ipp32s      bitoff;         /* mostly point to next frame header or PSC */
    h263_VideoSequence    VideoSequence;
#ifdef _OMP_KARABAS
    vm_thread_priority mTreadPriority;
    h263_MacroBlockMT*  pMBinfoMT;  /* OpenMP buffer */
#endif // _OMP_KARABAS
    Ipp32s      number_threads;       /* OpenMP number of threads */
    Ipp32s      stopOnErr;
#ifndef UMC_H263_DISABLE_SORENSON
    Ipp32s      sorenson;
#endif
} h263_Info;

/* bitstream functions */
extern Ipp32s     h263_RemainStream(h263_Info* pInfo);
extern Ipp8u*     h263_FindStartCodePtr(h263_Info* pInfo);
extern Ipp32s     h263_SeekStartCodePtr(h263_Info* pInfo);
extern Ipp32s h263_SeekGOBStartCodePtr(h263_Info* pInfo);
/* extern Ipp32s     h263_CheckResyncMarker(h263_Info* pInfo, Ipp32s rml); */

/* tables */
typedef struct _h263_VLC1 {
    Ipp8u  code;
    Ipp8u  len;
} h263_VLC1;
extern const Ipp8u h263_cCbCrMvRound16[];
extern const Ipp8s h263_dquant[];
extern const Ipp8s h263_dquant_Mod[2][31];
extern const Ipp8u h263_quant_c[];
extern const h263_VLC1 h263_cbpy[];
extern const Ipp8u h263_Pmb_type[];
extern const Ipp8u h263_Pmb_cbpc[];
extern const Ipp8u h263_Pmb_bits[];
extern const h263_VLC1 h263_Bmb_type[];
extern const h263_VLC1 h263_Bmb_cbpc[];
extern const h263_VLC1 h263_EPmb_type_0[];
extern const h263_VLC1 h263_EPmb_type_1[];
extern const h263_VLC1 h263_EImb_type[];
extern const h263_VLC1 h263_MVD_T14_1[];
extern const h263_VLC1 h263_MVD_T14_2[];
extern const Ipp32s h263_pic_width[];
extern const Ipp32s h263_pic_height[];
extern const Ipp32s h263_mblocks_in_gob[];
extern const Ipp32s h263_gobs_in_pic[];

// project functions
//extern void       h263_Error(Ipp8s *str);
#define h263_Error(str)
extern h263_Status h263_InitDecoder(h263_Info *pInfo, Ipp8s *h263FileName);
extern h263_Status h263_CloseDecoder(h263_Info *pInfo);
extern h263_Status h263_InitVSeq(h263_Info *pInfo, Ipp32s mbPerRow, Ipp32s mbPerCol);
extern h263_Status h263_FreeVSeq(h263_Info *pInfo);
extern h263_Status h263_Decoder(h263_Info *pInfo);

extern h263_Status h263_Parse_PictureHeader(h263_Info* pInfo);
#ifndef UMC_H263_DISABLE_SORENSON
extern h263_Status h263_Parse_PictureHeader_Sorenson(h263_Info* pInfo);
#endif
extern Ipp32s h263_ParseGOBHeader(h263_Info* pInfo);

extern h263_Status h263_DecodeVideoPicture(h263_Info* pInfo);

extern Ipp32s h263_Check_Resize(h263_Info* pInfo);

#ifdef _OMP_KARABAS
#ifdef _OPENMP
#include <omp.h>
#endif
extern Ipp32s h263_GetNumOfThreads(void);
#endif // _OMP_KARABAS

#ifndef H263_USE_INLINE_BITS_FUNC
extern Ipp32u h263_ShowBits(h263_Info* pInfo, Ipp32s n);
extern Ipp32u h263_ShowBit(h263_Info* pInfo);
extern Ipp32u h263_ShowBits9(h263_Info* pInfo, Ipp32s n);
extern void   h263_FlushBits(h263_Info* pInfo, Ipp32s n);
extern Ipp32u h263_GetBits(h263_Info* pInfo, Ipp32s n);
extern Ipp32u h263_GetBits9(h263_Info* pInfo, Ipp32s n);
extern void   h263_AlignBits(h263_Info* pInfo);
extern Ipp32u h263_ShowBitsAlign(h263_Info* pInfo, Ipp32s n);
#else

__INLINE Ipp32u h263_ShowBits(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp <<= pInfo->bitoff;
    tmp >>= 32 - n;
    return tmp;
}

__INLINE Ipp32u h263_ShowBit(h263_Info* pInfo)
{
    Ipp32u tmp = pInfo->bufptr[0];
    tmp >>= 7 - pInfo->bitoff;
    return (tmp & 1);
}

__INLINE Ipp32u h263_ShowBits9(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] <<  8) | ptr[1];
    tmp <<= (pInfo->bitoff + 16);
    tmp >>= 32 - n;
    return tmp;
}

__INLINE void h263_FlushBits(h263_Info* pInfo, Ipp32s n)
{
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
}

__INLINE Ipp32u h263_GetBits(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp <<= pInfo->bitoff;
    tmp >>= 32 - n;
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
    return tmp;
}

__INLINE Ipp32u h263_GetBits9(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] <<  8) | ptr[1];
    tmp <<= (pInfo->bitoff + 16);
    tmp >>= 32 - n;
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
    return tmp;
}

__INLINE void h263_AlignBits(h263_Info* pInfo)
{
    if (pInfo->bitoff > 0) {
        pInfo->bitoff = 0;
        (pInfo->bufptr)++;
    }
}

__INLINE Ipp32u h263_ShowBitsAlign(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bitoff ? (pInfo->bufptr + 1) : pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp >>= 32 - n;
    return tmp;
}

#endif // H263_USE_INLINE_BITS_FUNC

__INLINE Ipp32u h263_GetBit(h263_Info* pInfo)
{
    Ipp32u tmp = pInfo->bufptr[0];
    if (pInfo->bitoff != 7) {
        tmp >>= 7 - pInfo->bitoff;
        pInfo->bitoff ++;
    } else {
        pInfo->bitoff = 0;
        pInfo->bufptr ++;
    }
    return (tmp & 1);
}

__INLINE Ipp32s h263_GetMarkerBit(h263_Info* pInfo) {
    if (!h263_GetBit(pInfo)) {
        h263_Error("Error in video_header: wrong marker bit");
        return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif  /* _H263_H_  */
