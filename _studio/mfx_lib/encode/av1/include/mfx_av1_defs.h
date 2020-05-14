// Copyright (c) 2014-2020 Intel Corporation
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

#ifndef __MFX_AV1_DEFS_H__
#define __MFX_AV1_DEFS_H__

#include <immintrin.h>

#pragma warning(disable: 4100; disable: 4505; disable: 4127; disable: 4324)

#include "emmintrin.h"
#include "float.h"
#include "assert.h"
#include "ippdefs.h"
#include "ipps.h"
#include "ippi.h"
#include "mfxdefs.h"
#include "mfxla.h"
#include "mfx_ext_buffers.h"

#include <atomic>


#if defined(_WIN32) || defined(_WIN64)
#define AV1_FORCEINLINE __forceinline
#define AV1_NONLINE __declspec(noinline)
#else
#ifdef __INTEL_COMPILER
#define AV1_FORCEINLINE __forceinline
#define AV1_NONLINE __attribute__((noinline))
#else
#define AV1_FORCEINLINE __attribute__((always_inline))
#define AV1_NONLINE __attribute__((noinline))
#endif
#endif

#define AV1_Malloc  ippMalloc
#define AV1_Free    ippFree
#define AV1_SafeFree(p) if (p) AV1_Free(p); p = NULL

#define PARALLEL_LOW_DELAY 1
#if PARALLEL_LOW_DELAY
#define PARALLEL_LOW_DELAY_FAST
#endif

#define LOW_COMPLX_PAQ
#define AMT_HROI_PSY_AQ
#define ZERO_DELAY_ANALYZE_CMPLX
#define ADAPTIVE_DEADZONE
#define AMT_MAX_FRAME_SIZE
#define ENABLE_AV1_ALTR 1
#define ENABLE_AV1_ALTR_AUTOQ 0
#define ENABLE_AV1_ALTR_APQ0 1
#define ENABLE_10BIT    0
#define ENABLE_10BIT_SIMPLE    1
#define ENABLE_HIBIT_COEFS 1
#define ENABLE_SW_ME    0
#define ENABLE_PX_CODE  0
#define ENABLE_QPMAP    0
#define ENABLE_BRC      1
#define ENABLE_PRECARRY_BUF 0
#define ENABLE_BITSTREAM_MEM_REDUCTION 1
#define ENABLE_ONLY_HASH8   1
#if ENABLE_ONLY_HASH8
#define ENABLE_HASH_MEM_REDUCTION 1
#define HASH_WORK_LINES 64
#else
#define ENABLE_HASH_MEM_REDUCTION 0
#endif
#define ENABLE_TPLMV 0
// Use MePu8/16 kernel, and common pred buf for MePu & Md
#define NEWMVPRED 0

//#define REINTERP_MEPU
//#define USE_SAD_ONLY
//#define USE_OLD_IPB_QINDEX
//#define MEMOIZE_SUBPEL_TEST

//#define MEMOIZE_SUBPEL_EXT_W (8+8)
#define MEMOIZE_SUBPEL_EXT_H (8+2)
//#define MEMOIZE_NUMCAND      5

#define HEVC_ANALYSE_CHROMA (1 << 0)
#define HEVC_COST_CHROMA    (1 << 1)

#define DATA_ALIGN 64
#define H265ENC_UNREFERENCED_PARAMETER(X) X=X

#define MAX_TOTAL_DEPTH (6+4)

#define PROTOTYPE_GPU_MODE_DECISION 1
#define PROTOTYPE_GPU_MODE_DECISION_SW_PATH 0

#define GPU_INTRA_DECISION 1
#define GPU_VARTX 0 // duplicated in mfx_av1_fei.h

#if GPU_VARTX
#define ADAPTIVE_DEADZONE_TEMPORAL_ONLY
#endif

// HWPAK CONFIG
#define USE_CMODEL_PAK 0
#define USE_SRC10ENC8PAK10 1
#if USE_CMODEL_PAK || defined(PARALLEL_LOW_DELAY_FAST)
#define USE_HWPAK_RESTRICT 1
#else
#define USE_HWPAK_RESTRICT 0
#endif

#define PACK_BY_TILES 0

//#define DUMP_COSTS_CU
//#define DUMP_COSTS_TU

#define MAX_DOUBLE  1.7e+308    ///< max. value of double-type value
#define MAX_UINT    0xFFFFFFFFU ///< max. value of unsigned 32-bit integer
#define COST_MAX    FLT_MAX

#define PGOP_PIC_SIZE 4

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

#define MAX_NUM_DEPENDENCIES 16

#define CDF_SIZE(x) ((x) + 1)
#define AOM_ICDF(x) (32768U - (x))

#define AOM_CDF2(a0) \
    AOM_ICDF(a0), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1) \
    AOM_ICDF(a0), AOM_CDF2(a1)
#define AOM_CDF4(a0, a1, a2) \
    AOM_ICDF(a0), AOM_CDF3(a1, a2)
#define AOM_CDF5(a0, a1, a2, a3) \
    AOM_ICDF(a0), AOM_CDF4(a1, a2, a3)
#define AOM_CDF6(a0, a1, a2, a3, a4) \
    AOM_ICDF(a0), AOM_CDF5(a1, a2, a3, a4)
#define AOM_CDF7(a0, a1, a2, a3, a4, a5) \
    AOM_ICDF(a0), AOM_CDF6(a1, a2, a3, a4, a5)
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6) \
    AOM_ICDF(a0), AOM_CDF7(a1, a2, a3, a4, a5, a6)
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7) \
    AOM_ICDF(a0), AOM_CDF8(a1, a2, a3, a4, a5, a6, a7)
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8) \
    AOM_ICDF(a0), AOM_CDF9(a1, a2, a3, a4, a5, a6, a7, a8)
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    AOM_ICDF(a0), AOM_CDF10(a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    AOM_ICDF(a0), AOM_CDF11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    AOM_ICDF(a0), AOM_CDF12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    AOM_ICDF(a0), AOM_CDF13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
    AOM_ICDF(a0), AOM_CDF14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13)
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) \
    AOM_ICDF(a0), AOM_CDF15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14)

// aya: c-model cmp issue
#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))
#define ROUND_POWER_OF_TWO_SIGNED(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO(-(value), (n)) \
                 : ROUND_POWER_OF_TWO((value), (n)))


namespace AV1Enc {

    const int32_t LOG2_MAX_CU_SIZE        = 6;
    const int32_t MAX_CU_SIZE             = 1 << LOG2_MAX_CU_SIZE;   // maximum allowable size of CU
    const int32_t LOG2_MAX_NUM_PARTITIONS = 8;
    const int32_t MAX_NUM_PARTITIONS      = 1 << LOG2_MAX_NUM_PARTITIONS;
    const int32_t SRC_PITCH = 64;
    const int32_t PRED_PITCH = 64;
    const int32_t BUF_PITCH = 64;

    static inline void AV1_FORCEINLINE small_memcpy(void* dst, const void* src, int len ) { ippsCopy_8u((const uint8_t*)src, (uint8_t*)dst, len); }

    template<class T> inline T AlignValue(T value, mfxU32 alignment)
    {
        assert((alignment & (alignment - 1)) == 0); // should be 2^n
        return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
    }

    static inline IppStatus _ippsSet(uint8_t val, uint8_t* pDst, int len) {
        //return ippsSet_8u(val, pDst, len);
		return std::fill(pDst, pDst + len, val), ippStsNoErr;
    }
    static inline IppStatus _ippsSet(uint16_t val, uint16_t* pDst, int len) {
        //return ippsSet_16s(val, (int16_t*)pDst, len);
		return std::fill(pDst, pDst + len, val), ippStsNoErr;
    }
    static inline IppStatus _ippsSet(uint32_t val, uint32_t* pDst, int len) {
        //return ippsSet_32s(val, (int32_t*)pDst, len);
		return std::fill(pDst, pDst + len, val), ippStsNoErr;
    }

    static inline IppStatus _ippsCopy(const uint8_t* pSrc, uint8_t* pDst, int len) {
        //return ippsCopy_8u(pSrc, pDst, len);
		return std::copy(pSrc, pSrc + len, pDst), ippStsNoErr;
    }
    static inline IppStatus _ippsCopy(const uint16_t* pSrc, uint16_t* pDst, int len) {
        //return ippsCopy_16s((int16_t*)pSrc, (int16_t*)pDst, len);
		return std::copy(pSrc, pSrc + len, pDst), ippStsNoErr;
    }
	static inline IppStatus _ippsCopy(const uint32_t* pSrc, uint32_t* pDst, int len) {
		//return ippsCopy_32s((int32_t*)pSrc, (int32_t*)pDst, len);
		return std::copy(pSrc, pSrc + len, pDst), ippStsNoErr;
	}

    static inline IppStatus _ippsCopy(const int16_t* pSrc, int16_t* pDst, int len) {
        //return ippsCopy_16s(pSrc, pDst, len);
		return std::copy(pSrc, pSrc + len, pDst), ippStsNoErr;
    }

    enum {
        MAX_NUM_ACTIVE_REFS = 15,
        MAX_DPB_SIZE = 16
    };


    enum {
        PARTS_SQUARE_8x8=1,     // square partitions 64x64 to 8x8
        PARTS_SQUARE_4x4=2,     // square partitions 64x64 to 4x4
        PARTS_ALL=3,            // all partitions, including rectangular
    };

    typedef int16_t CoeffsType;
    typedef float CostType;

    const int32_t MAX_NUM_REF_IDX     = 4;
    const int32_t OPT_LAMBDA_PYRAMID  = 1;

    const int32_t MAX_DQP     = 6; // PAQ/CALQ tools
    const int32_t SIZE_BLK_LA = 8; // size of block for lookahead algorithms
    const int32_t LOG2_SIZE_BLK_LA = 1; // log2(SIZE_BLK_LA) - 2

    const int32_t LOG2_MIN_TU_SIZE = 2;

    // texture component type
    enum EnumTextType
    {
        TEXT_LUMA,            ///< luma
        TEXT_CHROMA,          ///< chroma (U+V)
        TEXT_CHROMA_U,        ///< chroma U
        TEXT_CHROMA_V,        ///< chroma V
        TEXT_ALL,             ///< Y+U+V
        TEXT_NONE = 15
    };

    /// reference list index
    enum EnumRefPicList
    {
        REF_PIC_LIST_0 = 0,   ///< reference list 0
        REF_PIC_LIST_1 = 1,   ///< reference list 1
        REF_PIC_LIST_C = 2,   ///< combined reference list for uni-prediction in B-Slices
        REF_PIC_LIST_X = 100  ///< special mark
    };

    // supported prediction type
    enum PredictionMode
    {
        MODE_INTER,           ///< inter-prediction mode
        MODE_INTRA,           ///< intra-prediction mode
        MODE_NONE = 15
    };

    /// supported partition shape
    enum PartitionSize
    {
        PART_SIZE_2Nx2N,           ///< symmetric motion partition,  2Nx2N
        PART_SIZE_2NxN,            ///< symmetric motion partition,  2Nx N
        PART_SIZE_Nx2N,            ///< symmetric motion partition,   Nx2N
        PART_SIZE_NxN,             ///< symmetric motion partition,   Nx N
        PART_SIZE_2NxnU,           ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
        PART_SIZE_2NxnD,           ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
        PART_SIZE_nLx2N,           ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
        PART_SIZE_nRx2N,           ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
        PART_SIZE_NONE
    };

    enum IntraPredOpt
    {
        INTRA_PRED_CALC,
        INTRA_PRED_IN_REC,
        INTRA_PRED_IN_BUF,
        INTER_PRED_IN_BUF,
    };

    static const int32_t MVP_LX_FLAG_BITS = 1;

    enum InterDir {
        INTER_DIR_PRED_L0 = 1,
        INTER_DIR_PRED_L1 = 2,
    };

    enum {
        AMT_DQP_CAQ = 0x1,
        AMT_DQP_CAL = 0x2,
        AMT_DQP_PAQ = 0x4,
        AMT_DQP_PSY = 0x8,
        AMT_DQP_HROI = 0x10
    };

#define AMT_DQP_PSY_HROI (AMT_DQP_PSY|AMT_DQP_HROI)

    class Frame;

    struct RefPicList
    {
        Frame *m_refFrames[MAX_NUM_ACTIVE_REFS];
        int8_t m_deltaPoc[MAX_NUM_ACTIVE_REFS];
        uint8_t m_isLongTermRef[MAX_NUM_ACTIVE_REFS];
        // extra details for frame threading
        int32_t m_refFramesCount; // number of reference frames in m_refFrames[]. must be the MAX (slice[sliceIdx]->num_ref_idx[ listIdx ], ...)
        int32_t m_listModFlag;
        int8_t m_listMod[MAX_NUM_ACTIVE_REFS];
    };


    typedef enum {
        TT_INIT_NEW_FRAME,
        TT_GPU_SUBMIT,
        TT_GPU_WAIT,
        TT_ENCODE_CTU,
        TT_DEBLOCK_AND_CDEF,
        TT_CDEF_SYNC,
        TT_CDEF_APPLY,
        TT_ENC_COMPLETE,
        TT_HUB,
        TT_PREENC_START,
        TT_PREENC_ROUTINE,
        TT_PREENC_END,
        TT_COMPLETE,
        TT_REPEAT_FRAME,
        TT_PACK_HEADER,
        TT_PACK_TILE,
        TT_PACK_ROW,
        TT_DETECT_SCROLL_START,
        TT_DETECT_SCROLL_ROUTINE,
        TT_DETECT_SCROLL_END,
        TT_COUNT
    } ThreadingTaskSpecifier;


    class AV1Encoder;
    class Lookahead;

    struct ThreadingTask
    {
        ThreadingTask()
        {
            action = TT_COUNT;
            numUpstreamDependencies = 0;
            numDownstreamDependencies = 0;
            finished = 0;
        }

        ThreadingTaskSpecifier action = TT_COUNT;
        std::atomic<uint32_t> finished{ 0 };

        union {
            struct {
                int32_t col;            // for encode, postproc, lookahead
                int32_t row;            // for encode, postproc, lookahead
            };
            struct {
                int32_t feiOp;          // for gpu-submit/gpu-wait
                int16_t refIdx;         // for gpu-submit/gpu-wait
                int16_t sliceIdx;       // for gpu-submit/gpu-wait
            };
            struct {
                int16_t tileRow;        // for pack-row
                int16_t tileCol;        // for pack-row
                int32_t sbRow;          // for pack-row
            };
            struct {
                int32_t startX;          // for detect-scroll
                int32_t endX;            // for detect-scroll
            };
            int32_t tile;               // for pack-tile
            mfxFrameSurface1 *indata;   // for init-new-frame

            void *ptr1;                 // reserved for zeroing
        };

        union {
            Lookahead *la;              // for lookahead
            Frame *frame;               // for gpu-submit/init-new-frame/enc-complete/detect-scroll
            void *syncpoint;            // for gpu-wait
            void *ptr0;                 // reserved for zeroing
        };
        int32_t poc;                     // for all tasks, useful in debug


        std::atomic<unsigned int> numUpstreamDependencies{ 0 };
        std::atomic<int> numDownstreamDependencies{ 0 };
        ThreadingTask * downstreamDependencies[MAX_NUM_DEPENDENCIES] = { nullptr };


        // very useful for debug
#ifdef DEBUG_NTM
        ThreadingTask *upstreamDependencies[MAX_NUM_DEPENDENCIES];
#endif

        void CommonInit(ThreadingTaskSpecifier action_) {
            action = action_;
            numUpstreamDependencies = 0;
            numDownstreamDependencies = 0;
            finished = 0;
        }

        void Init(ThreadingTaskSpecifier action_, int32_t row_, int32_t col_, int32_t poc_, int32_t numUpstreamDependencies_, int32_t numDownstreamDependencies_) {
            assert(action_ != TT_GPU_SUBMIT && action_ != TT_GPU_WAIT && action_ != TT_INIT_NEW_FRAME);
            action = action_;
            row = row_;
            col = col_;
            poc = poc_;
            numUpstreamDependencies = numUpstreamDependencies_;
            numDownstreamDependencies = numDownstreamDependencies_;
            finished = 0;
        }

        void InitGpuSubmit(Frame *frame_, int32_t feiOp_, int32_t poc_) {
            CommonInit(TT_GPU_SUBMIT);
            frame = frame_;
            feiOp = feiOp_;
            poc = poc_;
            refIdx = 0;
            sliceIdx = 0;
        }

        void InitGpuWait(int32_t feiOp_, int32_t poc_) {
            CommonInit(TT_GPU_WAIT);
            feiOp = feiOp_;
            poc = poc_;
            refIdx = 0;
            sliceIdx = 0;
            syncpoint = NULL;
        }

        void InitNewFrame(Frame *frame_, mfxFrameSurface1 *indata_, int32_t poc_) {
            CommonInit(TT_INIT_NEW_FRAME);
            frame = frame_;
            indata = indata_;
            poc = poc_;
        }

        void InitEncComplete(Frame *frame_, int32_t poc_) {
            CommonInit(TT_ENC_COMPLETE);
            poc = poc_;
            frame = frame_;
            ptr1 = NULL;
        }

        void InitRepeatFrame(Frame *frame_, int32_t poc_) {
            CommonInit(TT_REPEAT_FRAME);
            poc = poc_;
            frame = frame_;
            ptr1 = NULL;
        }

        void InitComplete(int32_t poc_) {
            CommonInit(TT_COMPLETE);
            poc = poc_;
            ptr0 = NULL;
            ptr1 = NULL;
        }
    };

    typedef enum {
        THREADING_WORKING,
        THREADING_PAUSE,
        THREADING_ERROR,
    } ThreadingStage;

    typedef enum {
        THREADING_ITASK_INI,
        THREADING_ITASK_WORKING,
        THREADING_ITASK_COMPLETE,
    } ThreadingInputTaskStage;

    inline int32_t H265_CeilLog2(int32_t a) {
        int32_t r = 0;
        while(a>(1<<r)) r++;
        return r;
    }

    enum {
        SUBPEL_NO               = 1, // intpel only
        SUBPEL_BOX_HPEL_ONLY    = 2, // no quaterpel step
        SUBPEL_BOX              = 3, // halfpel & quaterpel
        SUBPEL_DIA              = 4, // halfpel & quaterpel
        SUBPEL_DIA_2STEP        = 5, // 2*halfpel & 2*quaterpel
        SUBPEL_FASTBOX_DIA_ORTH = 6, // Fast Box Half Pel + Dia Quarter + Orthogonal Update
    };

    template <class T> void Throw(const T &ex)
    {
        assert(!"exception");
        throw ex;
    }

    template <class T> void ThrowIf(bool cond, const T &ex)
    {
        if (cond)
            Throw(ex);
    }

    class NonCopyable
    {
    protected:
        NonCopyable() {}
        ~NonCopyable() {}
    private:
        NonCopyable(const NonCopyable&);
        const NonCopyable& operator =(const NonCopyable&);
    };

    namespace MfxEnumShortAliases {
        enum { NV12=MFX_FOURCC_NV12, NV16=MFX_FOURCC_NV16, P010=MFX_FOURCC_P010, P210=MFX_FOURCC_P210 };

        enum { YUV420=MFX_CHROMAFORMAT_YUV420, YUV422=MFX_CHROMAFORMAT_YUV422 };

        enum { MAIN=MFX_PROFILE_HEVC_MAIN, MAIN10=MFX_PROFILE_HEVC_MAIN10, REXT=MFX_PROFILE_HEVC_REXT };

        enum { M10=MFX_LEVEL_HEVC_1, M20=MFX_LEVEL_HEVC_2, M21=MFX_LEVEL_HEVC_21, M30=MFX_LEVEL_HEVC_3, M31=MFX_LEVEL_HEVC_31, M40=MFX_LEVEL_HEVC_4, M41=MFX_LEVEL_HEVC_41,
            M50=MFX_LEVEL_HEVC_5, M51=MFX_LEVEL_HEVC_51, M52=MFX_LEVEL_HEVC_52, M60=MFX_LEVEL_HEVC_6, M61=MFX_LEVEL_HEVC_61, M62=MFX_LEVEL_HEVC_62,
            H40=M40|256, H41=M41|256, H50=M50|256, H51=M51|256, H52=M52|256, H60=M60|256, H61=M61|256, H62=M62|256 };

        enum { CBR=MFX_RATECONTROL_CBR, VBR=MFX_RATECONTROL_VBR, AVBR=MFX_RATECONTROL_AVBR, CQP=MFX_RATECONTROL_CQP, LA_EXT=MFX_RATECONTROL_LA_EXT };

        enum { ON=MFX_CODINGOPTION_ON, OFF=MFX_CODINGOPTION_OFF, UNK=MFX_CODINGOPTION_UNKNOWN, ADAPT=MFX_CODINGOPTION_ADAPTIVE};

        enum { SYSMEM=MFX_IOPATTERN_IN_SYSTEM_MEMORY, VIDMEM=MFX_IOPATTERN_IN_VIDEO_MEMORY, OPAQMEM=MFX_IOPATTERN_IN_OPAQUE_MEMORY };

        enum { MAX_12BIT=MFX_HEVC_CONSTR_REXT_MAX_12BIT, MAX_10BIT=MFX_HEVC_CONSTR_REXT_MAX_10BIT, MAX_8BIT=MFX_HEVC_CONSTR_REXT_MAX_8BIT,
            MAX_422=MFX_HEVC_CONSTR_REXT_MAX_422CHROMA, MAX_420=MFX_HEVC_CONSTR_REXT_MAX_420CHROMA, MAX_MONO=MFX_HEVC_CONSTR_REXT_MAX_MONOCHROME,
            CONSTR_INTRA=MFX_HEVC_CONSTR_REXT_INTRA, CONSTR_1PIC=MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY, CONSTR_LOWER_RATE=MFX_HEVC_CONSTR_REXT_LOWER_BIT_RATE };

        enum { MAIN_422_10=MAX_12BIT|MAX_10BIT|MAX_422|CONSTR_LOWER_RATE };

        enum { PROGR=MFX_PICSTRUCT_PROGRESSIVE, TFF=MFX_PICSTRUCT_FIELD_TFF, BFF=MFX_PICSTRUCT_FIELD_BFF };
    };

    namespace CodecLimits {
        using namespace MfxEnumShortAliases;
#ifdef MFX_VA
        const int32_t MAX_GOP_REF_DIST = 8;
        const int32_t SUP_ENABLE_CM[]  = { OFF, ON };
#else
        const int32_t MAX_GOP_REF_DIST = 16;
        const int32_t SUP_ENABLE_CM[]  = { OFF };
#endif

        const int32_t MIN_TARGET_USAGE       = 7;
        const int32_t MAX_TARGET_USAGE       = 7;
        const int32_t MAX_WIDTH              = 8192;
        const int32_t MAX_HEIGHT             = 4320;
        const int32_t MIN_WIDTH_IMPL         = 128; // implementation specific constraints
        const int32_t MIN_HEIGHT_IMPL        = 64;  // implementation specific constraints
        const int32_t VP9_MAX_NUM_TILE_COLS  = 20;
        const int32_t VP9_MAX_NUM_TILE_ROWS  = 22;
        const int32_t AV1_MAX_NUM_TILE_COLS  = 64;
        const int32_t AV1_MAX_NUM_TILE_ROWS  = 64;
        const int32_t MIN_TILE_WIDTH         = 256;
        const int32_t MIN_TILE_WIDTH_IMPL    = 256; // implementation specific constraints
        const int32_t MIN_TILE_HEIGHT        = 64;
        const int32_t MAX_NUM_SLICE          = (MAX_HEIGHT + 63) >> 6;
        const int32_t SUP_GOP_OPT_FLAG[]     = { MFX_GOP_CLOSED, MFX_GOP_STRICT, MFX_GOP_CLOSED|MFX_GOP_STRICT };
        const int32_t SUP_PROFILE[]          = { MAIN, MAIN10, REXT };
        const int32_t SUP_LEVEL[]            = { M10, M20, M21, M30, M31, M40, M41, M50, M51, M52, M60, M61, M62, H40, H41, H50, H51, H52, H60, H61, H62 };
        const int32_t SUP_RC_METHOD[]        = { CBR, VBR, AVBR, LA_EXT, CQP};
        const uint32_t SUP_FOURCC[]          = { NV12, NV16, P010, P210 };
        const int32_t SUP_CHROMA_FORMAT[]    = { YUV420, YUV422 };
        const int32_t SUP_PIC_STRUCT[]       = { PROGR, TFF, BFF };
        const int32_t SUP_INTRA_ANG_MODE_I[] = { 1, 2, 3, 99 };
        const int32_t SUP_INTRA_ANG_MODE[]   = { 1, 2, 3, 99, 100 };
        const int32_t SUP_PATTERN_INT_PEL[]  = { 1, 100};
    };

    template<class T> inline void Zero(T &obj) { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Copy(T &dst, const T &src) { memmove(&dst, &src, sizeof(src)); }

    template<class T> struct Type2Id;
    template<> struct Type2Id<mfxExtOpaqueSurfaceAlloc> { enum { id = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION }; };
    template<> struct Type2Id<mfxExtCodingOptionAV1E>   { enum { id = MFX_EXTBUFF_AV1ENC }; };
    template<> struct Type2Id<mfxExtDumpFiles>          { enum { id = MFX_EXTBUFF_DUMP }; };
    template<> struct Type2Id<mfxExtHEVCTiles>          { enum { id = MFX_EXTBUFF_HEVC_TILES }; };
    template<> struct Type2Id<mfxExtHEVCRegion>         { enum { id = MFX_EXTBUFF_HEVC_REGION }; };
    template<> struct Type2Id<mfxExtHEVCParam>          { enum { id = MFX_EXTBUFF_HEVC_PARAM }; };
    template<> struct Type2Id<mfxExtCodingOption>       { enum { id = MFX_EXTBUFF_CODING_OPTION }; };
    template<> struct Type2Id<mfxExtCodingOption2>      { enum { id = MFX_EXTBUFF_CODING_OPTION2 }; };
    template<> struct Type2Id<mfxExtCodingOption3>      { enum { id = MFX_EXTBUFF_CODING_OPTION3 }; };
    template<> struct Type2Id<mfxExtCodingOptionSPSPPS> { enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS }; };
    template<> struct Type2Id<mfxExtCodingOptionVPS>    { enum { id = MFX_EXTBUFF_CODING_OPTION_VPS }; };
    template<> struct Type2Id<mfxExtLAFrameStatistics>  { enum { id = MFX_EXTBUFF_LOOKAHEAD_STAT}; };
    template<> struct Type2Id<mfxExtEncoderROI>         { enum { id = MFX_EXTBUFF_ENCODER_ROI}; };
    template<> struct Type2Id<mfxExtAVCRefListCtrl>     { enum { id = MFX_EXTBUFF_AVC_REFLIST_CTRL}; };
    template<> struct Type2Id<mfxExtAvcTemporalLayers>  { enum { id = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS }; };
    template<> struct Type2Id<mfxExtEncoderCapability>  { enum { id = MFX_EXTBUFF_ENCODER_CAPABILITY }; };
    template<> struct Type2Id<mfxExtVP9Param>           { enum { id = MFX_EXTBUFF_VP9_PARAM }; };
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    template<> struct Type2Id<mfxExtAV1Param>           { enum { id = MFX_EXTBUFF_AV1_PARAM }; };
#endif

    template <class T> struct RemoveConst          { typedef T type; };
    template <class T> struct RemoveConst<const T> { typedef T type; };

    inline mfxExtBuffer *GetExtBufferById(mfxExtBuffer **extBuffer, int32_t numExtBuffer, uint32_t id)
    {
        if (extBuffer)
            for (int32_t i = 0; i < numExtBuffer; i++)
                if (extBuffer[i]->BufferId == id)
                    return extBuffer[i];
        return NULL;
    }

    struct ExtBufferProxy;
    template <class T> ExtBufferProxy GetExtBuffer(const T &par);
    struct ExtBufferProxy
    {
        template <class U> operator U *() {
            return reinterpret_cast<U *>(GetExtBufferById(m_extBuffer, m_numExtBuffer, Type2Id<typename RemoveConst<U>::type>::id));
        }

        template <class U> operator U &() {
            if (U *p = this->operator U *())
                return *p;
            throw std::runtime_error("ext buffer not found");
        }
    private:
        template <class T> explicit ExtBufferProxy(const T &par) : m_extBuffer(par.ExtParam), m_numExtBuffer(par.NumExtParam) {}
        template <class T> friend ExtBufferProxy GetExtBuffer(const T &par);

        mfxExtBuffer **m_extBuffer;
        int32_t         m_numExtBuffer;
    };
    template <class T> ExtBufferProxy GetExtBuffer(const T &par) { return ExtBufferProxy(par); }

    inline int32_t BSR(uint32_t u32, uint8_t &ZF) {
#if defined(_WIN32) || defined(_WIN64)
        unsigned long index;
#else
        unsigned int index;
#endif
        ZF = _BitScanReverse(&index, u32);
        return index;
    }

    inline int32_t BSR(uint32_t gt0) {
        assert(gt0 > 0);
        uint8_t unusedZF;
        return BSR(gt0, unusedZF);
    }

    typedef uint8_t InterpFilter;
    enum {
        EIGHTTAP=0,// => EIGHTTAP_REGULAR
        EIGHTTAP_SMOOTH,
        EIGHTTAP_SHARP,// => MULTITAP_SHARP
        BILINEAR,

        EIGHTTAP_SHARP_NEW,// => EIGHTTAP_SHARP
        FILTER_REGULAR_UV,
        FILTER_SMOOTH_UV,
        FILTER_SHARP_UV,
        FILTER_SMOOTH2_UV,

        INTERP_FILTERS_ALL,

        SWITCHABLE_FILTERS=BILINEAR,
        SWITCHABLE=(SWITCHABLE_FILTERS + 1),
        SWITCHABLE_FILTER_CONTEXTS=(SWITCHABLE_FILTERS + 1)*4,
        EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
    };

#define DEF_FILTER EIGHTTAP
#define DEF_FILTER_DUAL (DEF_FILTER+(DEF_FILTER<<4))
#define IBC_FILTER_DUAL (BILINEAR+(BILINEAR<<4))

    // VP9 specific
    const int32_t REFS_PER_FRAME         = 3;   // Each inter frame can use up to 3 frames for reference
    const int32_t MV_FP_SIZE             = 4;   // Number of values that can be decoded for mv_fr
    const int32_t MVREF_NEIGHBOURS       = 8;   // Number of positions to search in motion vector prediction
    const int32_t BLOCK_SIZE_GROUPS      = 4;   // Number of contexts when decoding intra_mode
    const int32_t PALETTE_BLOCK_SIZE_CONTEXTS = 7; // Screen Coding Tools

    const int32_t MAX_PALETTE            = 8;
    //const int32_t BLOCK_SIZES            = 13;  // Number of different block sizes used
    //const int32_t BLOCK_INVALID          = 14;  // Sentinel value to mark partition choices that are illegal
    const int32_t VP9_PARTITION_CONTEXTS = 16;  // Number of contexts when decoding partition
    const int32_t AV1_PARTITION_CONTEXTS = 20;  // Number of contexts when decoding partition
    const int32_t MI_SIZE                = 8;   // Smallest size of a mode info block
    const int32_t MI_SIZE_LOG2           = 3;
    const int32_t MIN_TILE_WIDTH_B64     = 4;   // Minimum width of a tile in units of superblocks (although tiles on the right hand edge can be narrower)
    const int32_t MAX_TILE_WIDTH_B64     = 64;  // Maximum width of a tile in units of superblocks
    const int32_t MAX_MV_REF_CANDIDATES  = 2;   // Number of motion vectors returned by find_mv_refs process
    const int32_t NUM_REF_FRAMES         = 8;   // Number of frames that can be stored for future reference
    const int32_t MAX_REF_FRAMES         = 4;   // Number of values that can be derived for ref_frame
    const int32_t IS_INTER_CONTEXTS      = 4;   // Number of contexts for is_inter
    const int32_t COMP_MODE_CONTEXTS     = 5;   // Number of contexts for comp_mode
    const int32_t VP9_REF_CONTEXTS       = 5;   // Number of contexts for single_ref and comp_ref
    const int32_t AV1_REF_CONTEXTS       = 3;   // Number of contexts for single_ref and comp_ref
    const int32_t COMP_REF_TYPE_CONTEXTS = 5;
    const int32_t UNI_COMP_REF_CONTEXTS  = 3;
    const int32_t NUM_DRL_INDICES        = 3;   // Number of DRL indices
    const int32_t MAX_SEGMENTS           = 8;   // Number of segments allowed in segmentation map
    const int32_t SEG_LVL_ALT_Q          = 0;   // Index for quantizer segment feature
    const int32_t SEG_LVL_ALT_L          = 1;   // Index for loop filter segment feature
    const int32_t SEG_LVL_REF_FRAME      = 2;   // Index for reference frame segment feature
    const int32_t SEG_LVL_SKIP           = 3;   // Index for skip segment feature
    const int32_t SEG_LVL_MAX            = 4;   // Number of segment features
    const int32_t BLOCK_TYPES            = 2;   // Number of different plane types (Y or UV)
    const int32_t REF_TYPES              = 2;   // Number of different prediction types (intra or inter)
    const int32_t COEF_BANDS             = 6;   // Number of coefficient bands
    const int32_t PREV_COEF_CONTEXTS     = 6;   // Number of contexts for decoding coefficients
    const int32_t UNCONSTRAINED_NODES    = 3;   // Number of coefficient probabilities that are directly transmitted
    const int32_t ENTROPY_NODES          = 11;
    const int32_t MODEL_NODES            = ENTROPY_NODES - UNCONSTRAINED_NODES;
    const int32_t TAIL_NODES             = MODEL_NODES + 1;
    const int32_t TX_SIZE_CONTEXTS       = 3;   // Number of contexts for transform size
    const int32_t AV1_INTERP_FILTER_CONTEXTS = SWITCHABLE_FILTER_CONTEXTS;
    const int32_t VP9_INTERP_FILTER_CONTEXTS = 4;
    const int32_t SKIP_CONTEXTS          = 3;   // Number of contexts for decoding skip
    const int32_t KF_MODE_CONTEXTS       = 5;
    //const int32_t PARTITION_TYPES        = 4;   // Number of values for partition
    //const int32_t TX_SIZES               = 4;   // Number of values for tx_size
    //const int32_t TX_MODES               = 5;   // Number of values for tx_mode
    //const int32_t DCT_DCT                = 0;   // Inverse transform rows with DCT and columns with DCT
    //const int32_t ADST_DCT               = 1;   // Inverse transform rows with DCT and columns with ADST
    //const int32_t DCT_ADST               = 2;   // Inverse transform rows with ADST and columns with DCT
    //const int32_t ADST_ADST              = 3;   // Inverse transform rows with ADST and columns with ADST
    //const int32_t TX_TYPES               = 4;
    //const int32_t MB_MODE_COUNT          = 14;  // Number of values for y_mode
    //const int32_t INTRA_MODES            = 10;  // Number of values for intra_mode
    //const int32_t INTER_MODES            = 4;   // Number of values for inter_mode
    const int32_t INTER_MODE_CONTEXTS    = 8;   // Number of contexts for inter_mode
    const int32_t COMP_NEWMV_CTXS        = 5;
    const int32_t TXB_SKIP_CONTEXTS      = 13;
    const int32_t EOB_COEF_CONTEXTS      = 22;
    const int32_t DC_SIGN_CONTEXTS       = 3;
    //const int32_t MV_JOINTS              = 4;   // Number of values for mv_joint
    //const int32_t MV_CLASSES             = 11;  // Number of values for mv_class
    const int32_t CLASS0_SIZE            = 2;   // Number of values for mv_class0_bit
    const int32_t MV_OFFSET_BITS         = 10;  // Maximum number of bits for decoding motion vectors
    const int32_t MAX_PROB               = 255; // Number of values allowed for a probability adjustment
    const int32_t MAX_MODE_LF_DELTAS     = 2;   // Number of different mode types for loop filtering
    const int32_t COMPANDED_MVREF_THRESH = 8;   // Threshold at which motion vectors are considered large
    const int32_t MAX_LOOP_FILTER        = 63;  // Maximum value used for loop filtering
    const int32_t REF_SCALE_SHIFT        = 14;  // Number of bits of precision when scaling reference frames
    const int32_t SUBPEL_BITS            = 4;   // Number of bits of precision when performing inter prediction
    const int32_t SUBPEL_SHIFTS          = 16;  // 1 << SUBPEL_BITS
    const int32_t SUBPEL_MASK            = 15;  // SUBPEL_SHIFTS - 1
    const int32_t MV_BORDER              = 128; // Value used when clipping motion vectors
    const int32_t MV_BORDER_AV1          = 128; // Value used when clipping motion vectors
    const int32_t INTERP_EXTEND          = 4;   // Value used when clipping motion vectors
    const int32_t BORDERINPIXELS         = 160; // Value used when clipping motion vectors
    const int32_t MAX_UPDATE_FACTOR      = 128; // Value used in adapting probabilities
    const int32_t COUNT_SAT              = 20;  // Value used in adapting probabilities
    const int32_t BOTH_ZERO              = 0;   // Both candidates use ZEROMV
    const int32_t ZERO_PLUS_PREDICTED    = 1;   // One candidate uses ZEROMV, one uses NEARMV or NEARESTMV
    const int32_t BOTH_PREDICTED         = 2;   // Both candidates use NEARMV or NEARESTMV
    const int32_t NEW_PLUS_NON_INTRA     = 3;   // One candidate uses NEWMV, one uses ZEROMV
    const int32_t BOTH_NEW               = 4;   // Both candidates use NEWMV
    const int32_t INTRA_PLUS_NON_INTRA   = 5;   // One candidate uses intra prediction, one uses inter prediction
    const int32_t BOTH_INTRA             = 6;   // Both candidates use intra prediction
    const int32_t INVALID_CASE           = 9;   // Sentinel value marking a case that can never occur
    const int32_t DIFF_UPDATE_PROB       = 252;
    const int32_t MAX_REF_LF_DELTAS      = 4;
    const int32_t CDEF_MAX_STRENGTHS     = 16;
    const int32_t CDEF_STRENGTH_BITS     = 6;
    const int32_t STRENGTH_COUNT_FAST    = 16;
    const int32_t CDF_PROB_BITS          = 15;
    const int32_t CDF_PROB_TOP           = (1 << CDF_PROB_BITS);
    const int32_t EC_PROB_SHIFT          = 6;
    const int32_t EC_MIN_PROB            = 4;  // must be <= (1<<EC_PROB_SHIFT)/16
    const int32_t CDF_SHIFT              = (15 - CDF_PROB_BITS);
    const int32_t HEAD_TOKENS            = 5;
    const int32_t TAIL_TOKENS            = 9;
    const int32_t ONE_TOKEN_EOB          = 1;
    const int32_t ONE_TOKEN_NEOB         = 2;
    const int32_t TWO_TOKEN_PLUS_EOB     = 3;
    const int32_t TWO_TOKEN_PLUS_NEOB    = 4;
    const int32_t EXT_TX_SETS_INTER      = 4;  // Sets of transform selections for INTER
    const int32_t EXT_TX_SETS_INTRA      = 3;  // Sets of transform selections for INTRA
    const int32_t MAX_VARTX_DEPTH        = 2;
    const int32_t MAX_ANGLE_DELTA        = 3;
    const int32_t DIRECTIONAL_MODES      = 8;
    const int32_t ANGLE_STEP             = 3;
    const int32_t MAX_MB_PLANE           = 3;
    const int32_t INTRA_EDGE_FILT        = 3;
    const int32_t INTRA_EDGE_TAPS        = 5;
    const int32_t MAX_UPSAMPLE_SZ        = 16;
    const int32_t TOKEN_CDF_Q_CTXS       = 4;
    const int32_t PRIMARY_REF_NONE       = 7;

    const int32_t TX_PAD_HOR_LOG2  = 2;
    const int32_t TX_PAD_HOR       = 4;
    const int32_t TX_PAD_TOP       = 4;
    const int32_t TX_PAD_BOTTOM    = 4;
    const int32_t TX_PAD_VER       = TX_PAD_TOP + TX_PAD_BOTTOM;
    const int32_t MAX_TX_SIZE_LOG2 = 6;
    const int32_t MAX_TX_SIZE      = 1 << MAX_TX_SIZE_LOG2;
    const int32_t TX_PAD_END       = 16; // Pad 16 extra bytes to avoid reading overflow in SIMD optimization.
    const int32_t TX_PAD_2D        = (MAX_TX_SIZE + TX_PAD_HOR) * (MAX_TX_SIZE + TX_PAD_VER) + TX_PAD_END;
    const int32_t MAX_TX_SQUARE    = MAX_TX_SIZE * MAX_TX_SIZE;

    const int32_t NUM_BASE_LEVELS    = 2;
    const int32_t BR_CDF_SIZE        = 4;
    const int32_t COEFF_BASE_RANGE   = 4 * (BR_CDF_SIZE - 1);
    const int32_t MAX_BASE_BR_RANGE  = COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1;
    const int32_t COEFF_CONTEXT_BITS = 6;
    const int32_t COEFF_CONTEXT_MASK = (1 << COEFF_CONTEXT_BITS) - 1;

    const int32_t MAX_TILE_WIDTH         = 4096;  // Max Tile width in pixels
    const int32_t MAX_TILE_AREA          = 4096 * 2304;  // Maximum tile area in pixels
    const uint16_t MAX_TILE_ROWS          = 64;
    const uint16_t MAX_TILE_COLS          = 64;

    const int32_t SIG_COEF_CONTEXTS_EOB  = 4;
    const int32_t SIG_COEF_CONTEXTS_2D   = 26;
    const int32_t SIG_COEF_CONTEXTS_1D   = 16;
    const int32_t SIG_COEF_CONTEXTS      = SIG_COEF_CONTEXTS_2D + SIG_COEF_CONTEXTS_1D;
    const int32_t LEVEL_CONTEXTS         = 21;

    const int32_t NEWMV_MODE_CONTEXTS = 7;
    const int32_t GLOBALMV_MODE_CONTEXTS = 2;
    const int32_t REFMV_MODE_CONTEXTS = 9;
    const int32_t DRL_MODE_CONTEXTS = 5;

    const int32_t GLOBALMV_OFFSET = 3;
    const int32_t REFMV_OFFSET = 4;

    const int32_t NEWMV_CTX_MASK = ((1 << GLOBALMV_OFFSET) - 1);
    const int32_t GLOBALMV_CTX_MASK = ((1 << (REFMV_OFFSET - GLOBALMV_OFFSET)) - 1);
    const int32_t REFMV_CTX_MASK = ((1 << (8 - REFMV_OFFSET)) - 1);

    const int32_t SKIP_NEARESTMV_OFFSET = 9;
    const int32_t SKIP_NEARMV_OFFSET = 10;
    const int32_t SKIP_NEARESTMV_SUB8X8_OFFSET = 11;

    const int32_t DELTA_Q_SMALL = 3;
    const int32_t DELTA_Q_CONTEXTS = (DELTA_Q_SMALL);
    const int32_t DEFAULT_DELTA_Q_RES_SHIFT = 2;
    const int32_t DEFAULT_DELTA_Q_RES = (1 << DEFAULT_DELTA_Q_RES_SHIFT);

    const int32_t COEFF_PROB_MODELS = 255;

    const int32_t FRAME_OFFSET_BITS = 7;
    const int32_t MAX_FRAME_DISTANCE = (1 << FRAME_OFFSET_BITS) - 1;

    const int32_t SUPERRES_SCALE_BITS = 3;
    const int32_t SCALE_NUMERATOR = 8;
    const int32_t SUPERRES_SCALE_DENOMINATOR_MIN = (SCALE_NUMERATOR + 1);
    const int32_t SUPERRES_SCALE_DENOMINATOR_DEFAULT = SUPERRES_SCALE_DENOMINATOR_MIN + (1 << SUPERRES_SCALE_BITS) - 1;
    const int32_t RS_SCALE_SUBPEL_BITS = 14;

    const int32_t RS_SUBPEL_BITS = 6;
    const int32_t RS_SCALE_EXTRA_BITS = (RS_SCALE_SUBPEL_BITS - RS_SUBPEL_BITS);
    const int32_t RS_SCALE_EXTRA_OFF = (1 << (RS_SCALE_EXTRA_BITS - 1));
    const int32_t RS_SCALE_SUBPEL_MASK = ((1 << RS_SCALE_SUBPEL_BITS) - 1);

    static const int INTRABC_DELAY_PIXELS = 256;  //  Delay of 256 pixels
    static const int INTRABC_DELAY_SB64 = (INTRABC_DELAY_PIXELS / 64);

    enum { KEY_FRAME=0, NON_KEY_FRAME=1 };

    enum { MV_COMP_VER=0, MV_COMP_HOR=1 };
    enum { COEFF_CONTEXTS=6, COEFF_CONTEXTS0=3};
#define BAND_COEFF_CONTEXTS(band)  ((band) == 0 ? COEFF_CONTEXTS0 : COEFF_CONTEXTS)

    enum { PLANE_TYPE_Y=0, PLANE_TYPE_UV=1, PLANE_TYPES };

    enum {
        EIGHTTAP_EIGHTTAP = EIGHTTAP | (EIGHTTAP << 4),
        EIGHTTAP_SHARP_EIGHTTAP_SHARP = EIGHTTAP_SHARP | (EIGHTTAP_SHARP << 4)
    };

    enum { INTRA_INTER_CONTEXTS=4, COMP_INTER_CONTEXTS=5 };

    enum { ONLY_4X4=0, ALLOW_8X8=1, ALLOW_16X16=2, ALLOW_32X32=3, TX_MODE_SELECT=4, TX_MODES=5 };

    typedef uint8_t TxSize;
    enum {
        TX_4X4,
        TX_8X8,
        TX_16X16,
        TX_32X32,
        TX_64X64,
        TX_4X8,
        TX_8X4,
        TX_8X16,
        TX_16X8,
        TX_16X32,
        TX_32X16,
        TX_32X64,
        TX_64X32,
        TX_4X16,
        TX_16X4,
        TX_8X32,
        TX_32X8,
        TX_16X64,
        TX_64X16,
        TX_SIZES_ALL,
        TX_SIZES = TX_4X8,
        TX_SIZES_LARGEST = TX_64X64,
        TX_INVALID = 255
    };

    typedef uint8_t TxClass;
    enum {
      TX_CLASS_2D = 0,
      TX_CLASS_HORIZ = 1,
      TX_CLASS_VERT = 2,
      TX_CLASSES = 3,
    };

    const int32_t TXFM_PARTITION_CONTEXTS = ((TX_SIZES - TX_8X8) * 6 - 3);
    const int32_t MAX_TX_DEPTH = 2;
    const int32_t MAX_TX_CATS  = TX_SIZES - 1;
    const int32_t EXT_TX_SIZES = 4;  // number of sizes that use extended transforms

    typedef uint8_t TxType;
    enum {
        DCT_DCT = 0,
        ADST_DCT = 1,
        DCT_ADST = 2,
        ADST_ADST = 3,
        FLIPADST_DCT = 4,
        DCT_FLIPADST = 5,
        FLIPADST_FLIPADST = 6,
        ADST_FLIPADST = 7,
        FLIPADST_ADST = 8,
        IDTX = 9,
        V_DCT = 10,
        H_DCT = 11,
        V_ADST = 12,
        H_ADST = 13,
        V_FLIPADST = 14,
        H_FLIPADST = 15,
        TX_TYPES,
        VP9_TX_TYPES = ADST_ADST + 1
    };

    typedef enum {
        // DCT only
        EXT_TX_SET_DCTONLY,
        // DCT + Identity only
        EXT_TX_SET_DCT_IDTX,
        // Discrete Trig transforms w/o flip (4) + Identity (1)
        EXT_TX_SET_DTT4_IDTX,
        // Discrete Trig transforms w/o flip (4) + Identity (1) + 1D Hor/vert DCT (2)
        EXT_TX_SET_DTT4_IDTX_1DDCT,
        // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver DCT (2)
        EXT_TX_SET_DTT9_IDTX_1DDCT,
        // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver (6)
        EXT_TX_SET_ALL16,
        EXT_TX_SET_TYPES
    } TxSetType;

    typedef uint8_t PartitionType;
    enum {
        PARTITION_NONE,
        PARTITION_HORZ,
        PARTITION_VERT,
        PARTITION_SPLIT,
        PARTITION_HORZ_A,  // HORZ split and the top partition is split again
        PARTITION_HORZ_B,  // HORZ split and the bottom partition is split again
        PARTITION_VERT_A,  // VERT split and the left partition is split again
        PARTITION_VERT_B,  // VERT split and the right partition is split again
        PARTITION_HORZ_4,  // 4:1 horizontal partition
        PARTITION_VERT_4,  // 4:1 vertical partition
        EXT_PARTITION_TYPES,
        PARTITION_TYPES = PARTITION_SPLIT + 1,
        PARTITION_INVALID = 255
    };

    typedef uint8_t BlockSize;
    enum BlockSize_ {
        BLOCK_4X4,
        BLOCK_4X8,
        BLOCK_8X4,
        BLOCK_8X8,
        BLOCK_8X16,
        BLOCK_16X8,
        BLOCK_16X16,
        BLOCK_16X32,
        BLOCK_32X16,
        BLOCK_32X32,
        BLOCK_32X64,
        BLOCK_64X32,
        BLOCK_64X64,
        BLOCK_64X128,
        BLOCK_128X64,
        BLOCK_128X128,
        BLOCK_4X16,
        BLOCK_16X4,
        BLOCK_8X32,
        BLOCK_32X8,
        BLOCK_16X64,
        BLOCK_64X16,
        BLOCK_32X128,
        BLOCK_128X32,
        BLOCK_SIZES_ALL,
        BLOCK_SIZES = BLOCK_4X16,
        BLOCK_LARGEST = BLOCK_SIZES - 1,
        BLOCK_INVALID = 255
    };

    typedef uint8_t vpx_prob;
    typedef uint16_t aom_cdf_prob;
    typedef int8_t RefIdx;

    typedef uint8_t PredMode;
    enum ePredMode {
        // Intra modes
        DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4, D117_PRED=5,
        D153_PRED=6, D207_PRED=7, D63_PRED=8, TM_PRED=9,
        // Inter modes
        NEARESTMV=10, NEARMV=11, ZEROMV=12, NEWMV=13,
        // helpers
        INTRA_MODES=TM_PRED+1,
        INTER_MODES=NEWMV-NEARESTMV+1,
        INTER_MODE_OFFSET=INTRA_MODES,
        MB_MODE_COUNT=NEWMV+1,
        OUT_OF_PIC=0xff,
        NEARESTMV_=0,
        NEARMV_=1,
        ZEROMV_=2,
        NEWMV_=3,
        NEARMV1_=4,
        NEARMV2_=5,
        NEAREST_NEWMV_=6,
        NEW_NEARESTMV_=7,
        NEAR_NEWMV_   =8,
        NEW_NEARMV_   =9,

        NEAR_NEWMV1_   =10,
        NEW_NEARMV1_   =11,
        NEAR_NEWMV2_   =12,
        NEW_NEARMV2_   =13,

        // AV1 new Intra modes
        SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12,
        UV_CFL_PRED=13,
        // AV1 shifted Inter modes
        AV1_NEARESTMV=13, AV1_NEARMV=14, AV1_ZEROMV=15, AV1_NEWMV=16,   // AV1
        //// AV1 compound Inter modes
        NEAREST_NEARESTMV, NEAR_NEARMV, NEAREST_NEWMV, NEW_NEARESTMV,
        NEAR_NEWMV, NEW_NEARMV, ZERO_ZEROMV, NEW_NEWMV,
        // AV1 helpers
        AV1_INTRA_MODES=PAETH_PRED+1,
        AV1_UV_INTRA_MODES=UV_CFL_PRED+1,
        //AV1_INTER_MODES=AV1_NEWMV-AV1_NEARESTMV+1,
        AV1_INTER_MODES=NEW_NEWMV-AV1_NEARESTMV+1,

        AV1_INTER_MODE_OFFSET=AV1_INTRA_MODES,
        AV1_MB_MODE_COUNT=NEW_NEWMV+1,
        AV1_INTER_COMPOUND_MODES=NEW_NEWMV-NEAREST_NEARESTMV+1,
    };

    enum : int8_t { INTRA_FRAME=-1, NONE_FRAME=-1, LAST_FRAME=0, GOLDEN_FRAME=1, ALTREF_FRAME=2, COMP_VAR0=3, COMP_VAR1=4 };

    enum { AV1_LAST_FRAME=0, AV1_LAST2_FRAME=1, AV1_LAST3_FRAME=2, AV1_GOLDEN_FRAME=3, AV1_BWDREF_FRAME=4, AV1_ALTREF2_FRAME=5, AV1_ALTREF_FRAME=6 };
    const int32_t FWD_REFS    = AV1_GOLDEN_FRAME - AV1_LAST_FRAME + 1;
    const int32_t BWD_REFS    = AV1_ALTREF_FRAME - AV1_BWDREF_FRAME + 1;
    const int32_t SINGLE_REFS = FWD_REFS + BWD_REFS;
    const int32_t COMP_REFS   = FWD_REFS * BWD_REFS;

    const int32_t INTER_REFS_PER_FRAME = AV1_ALTREF_FRAME - AV1_LAST_FRAME + 1;
    const int32_t TOTAL_REFS_PER_FRAME = AV1_ALTREF_FRAME - INTRA_FRAME + 1;

    const RefIdx av1_to_vp9_ref_frame_mapping[INTER_REFS_PER_FRAME] = {
        LAST_FRAME, LAST_FRAME, LAST_FRAME, GOLDEN_FRAME, ALTREF_FRAME, ALTREF_FRAME, ALTREF_FRAME
    };
    const RefIdx vp9_to_av1_ref_frame_mapping[3] = {
        AV1_LAST_FRAME, AV1_GOLDEN_FRAME, AV1_ALTREF_FRAME
    };

    enum {
        LAST_LAST2_FRAMES,     // { LAST_FRAME, LAST2_FRAME }
        LAST_LAST3_FRAMES,     // { LAST_FRAME, LAST3_FRAME }
        LAST_GOLDEN_FRAMES,    // { LAST_FRAME, GOLDEN_FRAME }
        BWDREF_ALTREF_FRAMES,  // { BWDREF_FRAME, ALTREF_FRAME }
        UNIDIR_COMP_REFS
    };

    constexpr int CFL_ALPHABET_SIZE_LOG2 = 4;
    constexpr int CFL_ALPHABET_SIZE = (1 << CFL_ALPHABET_SIZE_LOG2);
    constexpr int CFL_MAGS_SIZE = ((2 << CFL_ALPHABET_SIZE_LOG2) + 1);
    constexpr int CFL_IDX_U(int idx) { return idx >> CFL_ALPHABET_SIZE_LOG2; }
    constexpr int CFL_IDX_V(int idx) { return idx & (CFL_ALPHABET_SIZE - 1); }

    enum CFL_PRED_TYPE : uint8_t { CFL_PRED_U, CFL_PRED_V, CFL_PRED_PLANES };

    enum CFL_SIGN_TYPE  : uint8_t {
        CFL_SIGN_ZERO,
        CFL_SIGN_NEG,
        CFL_SIGN_POS,
        CFL_SIGNS
    };

    enum CFL_ALLOWED_TYPE  : uint8_t {
        CFL_DISALLOWED,
        CFL_ALLOWED,
        CFL_ALLOWED_TYPES
    };

    // CFL_SIGN_ZERO,CFL_SIGN_ZERO is invalid
    constexpr int CFL_JOINT_SIGNS = (CFL_SIGNS * CFL_SIGNS - 1);
    // CFL_SIGN_U is equivalent to (js + 1) / 3 for js in 0 to 8
    constexpr int CFL_SIGN_U(int js) { return ((js + 1) * 11) >> 5; }
    // CFL_SIGN_V is equivalent to (js + 1) % 3 for js in 0 to 8
    constexpr int CFL_SIGN_V(int js) {
        return (js + 1) - CFL_SIGNS * CFL_SIGN_U(js);
    }

    // There is no context when the alpha for a given plane is zero.
    // So there are 2 fewer contexts than joint signs.
    constexpr int CFL_ALPHA_CONTEXTS = (CFL_JOINT_SIGNS + 1 - CFL_SIGNS);
    constexpr int CFL_CONTEXT_U(int js) {
        return js + 1 - CFL_SIGNS;
    }
    // Also, the contexts are symmetric under swapping the planes.
    constexpr int CFL_CONTEXT_V(int js) {
        return CFL_SIGN_V(js) * CFL_SIGNS + CFL_SIGN_U(js) - CFL_SIGNS;
    }

    enum { SINGLE_REFERENCE=0, COMPOUND_REFERENCE=1, REFERENCE_MODE_SELECT=2 };

    enum { UNIDIR_COMP_REFERENCE=0, BIDIR_COMP_REFERENCE=1, COMP_REFERENCE_TYPES=2 };

    enum { MV_VALS = 32765, MV_MAX = 16383 };
    enum { MV_JOINT_ZERO=0, MV_JOINT_HNZVZ=1, MV_JOINT_HZVNZ=2, MV_JOINT_HNZVNZ=3, MV_JOINTS=4 };

    enum {
        MV_CLASS_0, MV_CLASS_1, MV_CLASS_2, MV_CLASS_3, MV_CLASS_4, MV_CLASS_5,
        MV_CLASS_6, MV_CLASS_7, MV_CLASS_8, MV_CLASS_9, MV_CLASS_10, MV_CLASS_11,
        AV1_MV_CLASSES = MV_CLASS_10 + 1
    };

    enum {
        ZERO_TOKEN        = 0,    // 0     Extra Bits 0+0
        ONE_TOKEN         = 1,    // 1     Extra Bits 0+1
        TWO_TOKEN         = 2,    // 2     Extra Bits 0+1
        THREE_TOKEN       = 3,    // 3     Extra Bits 0+1
        FOUR_TOKEN        = 4,    // 4     Extra Bits 0+1
        DCT_VAL_CATEGORY1 = 5,    // 5-6   Extra Bits 1+1
        DCT_VAL_CATEGORY2 = 6,    // 7-10  Extra Bits 2+1
        DCT_VAL_CATEGORY3 = 7,    // 11-18 Extra Bits 3+1
        DCT_VAL_CATEGORY4 = 8,    // 19-34 Extra Bits 4+1
        DCT_VAL_CATEGORY5 = 9,    // 35-66 Extra Bits 5+1
        DCT_VAL_CATEGORY6 = 10,   // 67+   Extra Bits 14+1
        NUM_TOKENS        = 11,
        EOB_TOKEN         = 11,   //  no more_coef
        EOSB_TOKEN        = 12,   //  end of superblock
        ENTROPY_TOKENS    = 12,
        BLOCK_Z_TOKEN     = 255
    };
    enum {
        NO_EOB    = 0,  // Not an end-of-block
        EARLY_EOB = 1,  // End of block before the last position
        LAST_EOB  = 2   // End of block in the last position (implicit)
    };

    const int32_t counter_to_context[19] = {
        BOTH_PREDICTED, NEW_PLUS_NON_INTRA, BOTH_NEW, ZERO_PLUS_PREDICTED, NEW_PLUS_NON_INTRA,
        INVALID_CASE, BOTH_ZERO, INVALID_CASE, INVALID_CASE, INTRA_PLUS_NON_INTRA,
        INTRA_PLUS_NON_INTRA, INVALID_CASE, INTRA_PLUS_NON_INTRA, INVALID_CASE, INVALID_CASE,
        INVALID_CASE, INVALID_CASE, INVALID_CASE, BOTH_INTRA
    };

    const TxSize tx_mode_to_biggest_tx_size[TX_MODES] = {
        TX_4X4,     // ONLY_4X4
        TX_8X8,     // ALLOW_8X8
        TX_16X16,   // ALLOW_16X16
        TX_32X32,   // ALLOW_32X32
        TX_32X32,   // TX_MODE_SELECT
    };

    const TxSize max_txsize_lookup[BLOCK_SIZES_ALL] = {
        //                   4X4
                             TX_4X4,
        // 4X8,    8X4,      8X8
        TX_4X4,    TX_4X4,   TX_8X8,
        // 8X16,   16X8,     16X16
        TX_8X8,    TX_8X8,   TX_16X16,
        // 16X32,  32X16,    32X32
        TX_16X16,  TX_16X16, TX_32X32,
        // 32X64,  64X32,
        TX_32X32,  TX_32X32,
        // 64X64
        TX_64X64,
        // 64x128, 128x64,   128x128
        TX_64X64,  TX_64X64, TX_64X64,
        // 4x16,   16x4,     8x32
        TX_4X4,    TX_4X4,   TX_8X8,
        // 32x8,   16x64     64x16
        TX_8X8,    TX_16X16, TX_16X16,
        // 32x128  128x32
        TX_32X32,  TX_32X32
    };

    const TxSize max_txsize_rect_lookup[BLOCK_SIZES_ALL] = {
        //                   4X4
                             TX_4X4,
        // 4X8,    8X4,      8X8
        TX_4X8,    TX_8X4,   TX_8X8,
        // 8X16,   16X8,     16X16
        TX_8X16,   TX_16X8,  TX_16X16,
        // 16X32,  32X16,    32X32
        TX_16X32,  TX_32X16, TX_32X32,
        // 32X64,  64X32,
        TX_32X64,  TX_64X32,
        // 64X64
        TX_64X64,
        // 64x128, 128x64,   128x128
        TX_64X64,  TX_64X64, TX_64X64,
        // 4x16,   16x4,     8x32
        TX_4X16,   TX_16X4,  TX_8X32,
        // 32x8
        TX_32X8,
        // 16x64,  64x16
        TX_16X64,  TX_64X16,
        // 32x128  128x32
        TX_32X64,  TX_64X32
    };

    const TxSize sub_tx_size_map[2][TX_SIZES_ALL] = {
        { // Intra
            TX_4X4,     // TX_4X4
            TX_4X4,     // TX_8X8
            TX_8X8,     // TX_16X16
            TX_16X16,   // TX_32X32
            TX_32X32,   // TX_64X64
            TX_4X4,     // TX_4X8
            TX_4X4,     // TX_8X4
            TX_8X8,     // TX_8X16
            TX_8X8,     // TX_16X8
            TX_16X16,   // TX_16X32
            TX_16X16,   // TX_32X16
            TX_32X32,   // TX_32X64
            TX_32X32,   // TX_64X32
            TX_4X8,     // TX_4X16
            TX_8X4,     // TX_16X4
            TX_8X16,    // TX_8X32
            TX_16X8,    // TX_32X8
            TX_16X32,   // TX_16X64
            TX_32X16,   // TX_64X16
        },
        { // Inter
            TX_4X4,     // TX_4X4
            TX_4X4,     // TX_8X8
            TX_8X8,     // TX_16X16
            TX_16X16,   // TX_32X32
            TX_32X32,   // TX_64X64
            TX_4X4,     // TX_4X8
            TX_4X4,     // TX_8X4
            TX_8X8,     // TX_8X16
            TX_8X8,     // TX_16X8
            TX_16X16,   // TX_16X32
            TX_16X16,   // TX_32X16
            TX_32X32,   // TX_32X64
            TX_32X32,   // TX_64X32
            TX_4X8,     // TX_4X16
            TX_8X4,     // TX_16X4
            TX_8X16,    // TX_8X32
            TX_16X8,    // TX_32X8
            TX_16X32,   // TX_16X64
            TX_32X16,   // TX_64X16
        }
    };

    const BlockSize txsize_to_bsize[TX_SIZES_ALL] = {
        BLOCK_4X4,    // TX_4X4
        BLOCK_8X8,    // TX_8X8
        BLOCK_16X16,  // TX_16X16
        BLOCK_32X32,  // TX_32X32
        BLOCK_4X8,    // TX_4X8
        BLOCK_8X4,    // TX_8X4
        BLOCK_8X16,   // TX_8X16
        BLOCK_16X8,   // TX_16X8
        BLOCK_16X32,  // TX_16X32
        BLOCK_32X16,  // TX_32X16
        BLOCK_4X16,   // TX_4X16
        BLOCK_16X4,   // TX_16X4
        BLOCK_8X32,   // TX_8X32
        BLOCK_32X8,   // TX_32X8
    };

    typedef uint8_t MotionMode;
    enum {
        SIMPLE_TRANSLATION = 0,
        OBMC_CAUSAL,  // 2-sided OBMC
        WARPED_CAUSAL,  // 2-sided WARPED
        MOTION_MODES
    };

    typedef uint8_t TransformationType;
    enum {
        IDENTITY = 0,      // identity transformation, 0-parameter
        TRANSLATION = 1,   // translational motion 2-parameter
        ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
        AFFINE = 3,        // affine, 6-parameter
        TRANS_TYPES,
    };

    const uint8_t depth_lookup[BLOCK_SIZES] = {5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0};
    const uint8_t tx_size_wide_unit[TX_SIZES_ALL] = { 1, 2, 4, 8, 16, 1, 2, 2, 4, 4, 8, 8, 16, 1, 4, 2, 8, 4, 16 };
    const uint8_t tx_size_high_unit[TX_SIZES_ALL] = { 1, 2, 4, 8, 16, 2, 1, 4, 2, 8, 4, 16, 8, 4, 1, 8, 2, 16, 4 };
    const uint8_t tx_size_wide[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64 };
    const uint8_t tx_size_high[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16 };
    const int16_t tx_size_2d[TX_SIZES_ALL] = { 16, 64, 256, 1024, 4096, 32, 32, 128, 128, 512, 512, 2048, 2048, 64, 64, 256, 256, 1024, 1024 };
    const int16_t max_eob[TX_SIZES_ALL]    = { 16, 64, 256, 1024, 1024, 32, 32, 128, 128, 512, 512, 1024, 1024, 64, 64, 256, 256,  512,  512 };
    const uint8_t tx_size_wide_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6 };
    const uint8_t tx_size_high_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4 };
    const uint8_t block_size_wide[BLOCK_SIZES_ALL] = { 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 64, 64, 64, 128, 128, 4, 16, 8, 32, 16, 64, 32, 128 };
    const uint8_t block_size_high[BLOCK_SIZES_ALL] = { 4, 8, 4, 8, 16, 8, 16, 32, 16, 32, 64, 32, 64, 128, 64, 128, 16, 4, 32, 8, 64, 16, 128, 32 };
    const uint16_t num_4x4_blocks_lookup[BLOCK_SIZES_ALL] = { 1, 2, 2, 4, 8, 8, 16, 32, 32, 64, 128, 128, 256, 512, 512, 1024, 4, 4, 16, 16, 64, 64, 256, 256 };
    const uint8_t block_size_wide_4x4[BLOCK_SIZES_ALL] = { 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 1, 4, 2, 8, 4, 16, 8, 32 };
    const uint8_t block_size_high_4x4[BLOCK_SIZES_ALL] = { 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16, 32, 16, 32, 4, 1, 8, 2, 16, 4, 32, 8 };
    const uint8_t block_size_wide_8x8[BLOCK_SIZES_ALL] = { 1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 1, 2, 1, 4, 2, 8, 4, 16 };
    const uint8_t block_size_high_8x8[BLOCK_SIZES_ALL] = { 1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16, 2, 1, 4, 1, 8, 2, 16, 4 };
    const uint8_t block_size_wide_4x4_log2[BLOCK_SIZES_ALL] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 0, 2, 1, 3, 2, 4, 3, 5 };
    const uint8_t block_size_high_4x4_log2[BLOCK_SIZES_ALL] = { 0, 1, 0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 2, 0, 3, 1, 4, 2, 5, 3 };

    const uint8_t mi_width_log2_lookup[BLOCK_SIZES] = { 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3 };
    const uint8_t mi_height_log2_lookup[] = { 0, 0, 0, 0, 1, 0, 1, 2, 1, 2, 3, 2, 3 };
    const uint8_t size_group_lookup[BLOCK_SIZES_ALL] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 0, 0, 1, 1, 2, 2, 3, 3 };
    const uint8_t num_pels_log2_lookup[BLOCK_SIZES_ALL] = {
        4,  5,
        5,  6,
        7,  7,
        8,  9,
        9,  10,
        11, 11,
        12, 13, 13, 14, 6,
        6,  8,
        8,  10,
        10, 12, 12
    };

    const uint8_t coefband_4x4[16] = {0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5};
    const uint8_t coefband_8x8plus[1024] = {
        0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
    };

    static const BlockSize ss_size_lookup[BLOCK_SIZES_ALL][2][2] = {
        //  ss_x == 0      ss_x == 0          ss_x == 1      ss_x == 1
        //  ss_y == 0      ss_y == 1          ss_y == 0      ss_y == 1
        { { BLOCK_4X4,     BLOCK_4X4     }, { BLOCK_4X4,     BLOCK_4X4   } },
        { { BLOCK_4X8,     BLOCK_4X4     }, { BLOCK_4X4,     BLOCK_4X4   } },
        { { BLOCK_8X4,     BLOCK_4X4     }, { BLOCK_4X4,     BLOCK_4X4   } },
        { { BLOCK_8X8,     BLOCK_8X4     }, { BLOCK_4X8,     BLOCK_4X4   } },
        { { BLOCK_8X16,    BLOCK_8X8     }, { BLOCK_4X16,    BLOCK_4X8   } },
        { { BLOCK_16X8,    BLOCK_16X4    }, { BLOCK_8X8,     BLOCK_8X4   } },
        { { BLOCK_16X16,   BLOCK_16X8    }, { BLOCK_8X16,    BLOCK_8X8   } },
        { { BLOCK_16X32,   BLOCK_16X16   }, { BLOCK_8X32,    BLOCK_8X16  } },
        { { BLOCK_32X16,   BLOCK_32X8    }, { BLOCK_16X16,   BLOCK_16X8  } },
        { { BLOCK_32X32,   BLOCK_32X16   }, { BLOCK_16X32,   BLOCK_16X16 } },
        { { BLOCK_32X64,   BLOCK_32X32   }, { BLOCK_16X64,   BLOCK_16X32 } },
        { { BLOCK_64X32,   BLOCK_64X16   }, { BLOCK_32X32,   BLOCK_32X16 } },
        { { BLOCK_64X64,   BLOCK_64X32   }, { BLOCK_32X64,   BLOCK_32X32 } },
        { { BLOCK_64X128,  BLOCK_64X64   }, { BLOCK_32X128,  BLOCK_32X64 } },
        { { BLOCK_128X64,  BLOCK_128X32  }, { BLOCK_64X64,   BLOCK_64X32 } },
        { { BLOCK_128X128, BLOCK_128X64  }, { BLOCK_64X128,  BLOCK_64X64 } },
        { { BLOCK_4X16,    BLOCK_4X8     }, { BLOCK_4X16,    BLOCK_4X8   } },
        { { BLOCK_16X4,    BLOCK_16X4    }, { BLOCK_8X4,     BLOCK_8X4   } },
        { { BLOCK_8X32,    BLOCK_8X16    }, { BLOCK_INVALID, BLOCK_4X16  } },
        { { BLOCK_32X8,    BLOCK_INVALID }, { BLOCK_16X8,    BLOCK_16X4  } },
        { { BLOCK_16X64,   BLOCK_16X32   }, { BLOCK_INVALID, BLOCK_8X32  } },
        { { BLOCK_64X16,   BLOCK_INVALID }, { BLOCK_32X16,   BLOCK_32X8  } },
        { { BLOCK_32X128,  BLOCK_32X64   }, { BLOCK_INVALID, BLOCK_16X64 } },
        { { BLOCK_128X32,  BLOCK_INVALID }, { BLOCK_64X32,   BLOCK_64X16 } },
    };

    static const TxSize uv_txsize_lookup[BLOCK_SIZES_ALL][TX_SIZES_ALL][2][2] = {
    //  ss_x == 0    ss_x == 0        ss_x == 1      ss_x == 1
    //  ss_y == 0    ss_y == 1        ss_y == 0      ss_y == 1
      {
          // BLOCK_2X2
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
          // BLOCK_2X4
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
          // BLOCK_2X4
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
        // BLOCK_4X4
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
        // BLOCK_4X8
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },  // used
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
    // BLOCK_8X4
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },  // used
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
      },
      {
    // BLOCK_8X8
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X8, TX_8X4 }, { TX_4X8, TX_4X4 } },
      },
      {
    // BLOCK_8X16
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },  // used
          { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X16, TX_8X8 }, { TX_4X8, TX_4X8 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X16, TX_8X8 }, { TX_4X16, TX_4X8 } },
          { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
      },
      {
    // BLOCK_16X8
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },  // used
          { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_16X8, TX_16X4 }, { TX_8X8, TX_8X4 } },
      },
      {
    // BLOCK_16X16
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
          { { TX_16X16, TX_16X8 }, { TX_8X16, TX_8X8 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      },
      {
    // BLOCK_16X32
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },  // used
          { { TX_16X32, TX_16X16 }, { TX_8X16, TX_8X16 } },
          { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
          { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X32, TX_8X16 }, { TX_8X32, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      },
      {
    // BLOCK_32X16
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
          { { TX_32X16, TX_16X8 }, { TX_16X16, TX_16X8 } },  // used
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
      },
      {
    // BLOCK_32X32
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
          { { TX_32X32, TX_16X16 }, { TX_16X16, TX_16X16 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_16X32, TX_16X16 }, { TX_16X32, TX_16X16 } },
          { { TX_32X16, TX_32X16 }, { TX_16X16, TX_16X16 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
      },
      {
    // BLOCK_32X64
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
          { { TX_32X32, TX_32X32 }, { TX_16X16, TX_16X16 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_16X32, TX_16X32 }, { TX_16X16, TX_16X16 } },
          { { TX_32X16, TX_32X16 }, { TX_16X16, TX_16X16 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
      },
      {
    // BLOCK_64X32
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
          { { TX_32X32, TX_16X16 }, { TX_32X32, TX_16X16 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_16X32, TX_16X16 }, { TX_16X32, TX_16X16 } },
          { { TX_32X16, TX_16X16 }, { TX_32X16, TX_16X16 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
      },
      {
    // BLOCK_64X64
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_16X16, TX_16X16 } },
          { { TX_32X32, TX_32X32 }, { TX_32X32, TX_32X32 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_16X32, TX_16X32 }, { TX_16X32, TX_16X32 } },
          { { TX_32X16, TX_32X16 }, { TX_32X16, TX_16X16 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_16X8, TX_16X8 } },
      },
      {
    // BLOCK_4X16
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X16, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X16, TX_4X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X4, TX_4X4 } },
      },
      {
    // BLOCK_16X4
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_16X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_8X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
          { { TX_16X4, TX_4X4 }, { TX_8X4, TX_4X4 } },
      },
      {
    // BLOCK_8X32
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_4X4, TX_4X4 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
          { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
          { { TX_8X16, TX_8X16 }, { TX_4X8, TX_4X8 } },
          { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
          { { TX_8X4, TX_8X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X32, TX_8X16 }, { TX_4X16, TX_4X16 } },
          { { TX_8X8, TX_8X8 }, { TX_4X8, TX_4X8 } },
      },
      {
    // BLOCK_32X8
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_8X8, TX_4X4 }, { TX_8X8, TX_4X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
          { { TX_16X8, TX_8X4 }, { TX_16X8, TX_8X4 } },
          { { TX_4X8, TX_4X4 }, { TX_4X8, TX_4X4 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X8, TX_8X4 }, { TX_8X8, TX_8X4 } },
          { { TX_32X8, TX_16X4 }, { TX_16X8, TX_16X4 } },
      },
      {
    // BLOCK_16X64
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_16X16 }, { TX_8X8, TX_8X8 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X16 }, { TX_8X16, TX_8X16 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X32, TX_16X32 }, { TX_8X16, TX_8X16 } },
          { { TX_16X16, TX_16X16 }, { TX_8X16, TX_8X16 } },
          { { TX_4X16, TX_4X16 }, { TX_4X16, TX_4X16 } },
          { { TX_16X4, TX_16X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X32, TX_8X32 }, { TX_8X32, TX_8X32 } },
          { { TX_16X8, TX_16X8 }, { TX_8X8, TX_8X8 } },
      },
      {
    // BLOCK_64X16
          { { TX_4X4, TX_4X4 }, { TX_4X4, TX_4X4 } },
          { { TX_8X8, TX_8X8 }, { TX_8X8, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
          { { TX_16X16, TX_8X8 }, { TX_16X16, TX_8X8 } },
          { { TX_4X8, TX_4X8 }, { TX_4X8, TX_4X8 } },
          { { TX_8X4, TX_8X4 }, { TX_8X4, TX_8X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_16X8, TX_16X8 }, { TX_16X8, TX_16X8 } },
          { { TX_16X16, TX_16X8 }, { TX_16X16, TX_16X8 } },
          { { TX_32X16, TX_16X8 }, { TX_32X16, TX_16X8 } },
          { { TX_4X16, TX_4X8 }, { TX_4X16, TX_4X8 } },
          { { TX_16X4, TX_16X4 }, { TX_16X4, TX_16X4 } },
          { { TX_8X16, TX_8X8 }, { TX_8X16, TX_8X8 } },
          { { TX_32X8, TX_32X8 }, { TX_32X8, TX_32X8 } },
      },
    };

    const int8_t mv_ref_blocks[BLOCK_SIZES][MVREF_NEIGHBOURS][2] = {
        {{-1, 0}, { 0,-1}, {-1,-1}, {-2, 0}, { 0,-2}, {-2,-1}, {-1,-2}, {-2,-2}},  // 4x4
        {{-1, 0}, { 0,-1}, {-1,-1}, {-2, 0}, { 0,-2}, {-2,-1}, {-1,-2}, {-2,-2}},  // 4x8
        {{-1, 0}, { 0,-1}, {-1,-1}, {-2, 0}, { 0,-2}, {-2,-1}, {-1,-2}, {-2,-2}},  // 8x4
        {{-1, 0}, { 0,-1}, {-1,-1}, {-2, 0}, { 0,-2}, {-2,-1}, {-1,-2}, {-2,-2}},  // 8x8
        {{ 0,-1}, {-1, 0}, { 1,-1}, {-1,-1}, { 0,-2}, {-2, 0}, {-2,-1}, {-1,-2}},  // 8x16
        {{-1, 0}, { 0,-1}, {-1, 1}, {-1,-1}, {-2, 0}, { 0,-2}, {-1,-2}, {-2,-1}},  // 16x8
        {{-1, 0}, { 0,-1}, {-1, 1}, { 1,-1}, {-1,-1}, {-3, 0}, { 0,-3}, {-3,-3}},  // 16x16
        {{ 0,-1}, {-1, 0}, { 2,-1}, {-1,-1}, {-1, 1}, { 0,-3}, {-3, 0}, {-3,-3}},  // 16x32
        {{-1, 0}, { 0,-1}, {-1, 2}, {-1,-1}, { 1,-1}, {-3, 0}, { 0,-3}, {-3,-3}},  // 32x16
        {{-1, 1}, { 1,-1}, {-1, 2}, { 2,-1}, {-1,-1}, {-3, 0}, { 0,-3}, {-3,-3}},  // 32x32
        {{ 0,-1}, {-1, 0}, { 4,-1}, {-1, 2}, {-1,-1}, { 0,-3}, {-3, 0}, { 2,-1}},  // 32x64
        {{-1, 0}, { 0,-1}, {-1, 4}, { 2,-1}, {-1,-1}, {-3, 0}, { 0,-3}, {-1, 2}},  // 64x32
        {{-1, 3}, { 3,-1}, {-1, 4}, { 4,-1}, {-1,-1}, {-1, 0}, { 0,-1}, {-1, 6}}   // 64x64
    };

    const uint8_t idx_n_column_to_subblock[4][2] = { {1, 2}, {1, 3}, {3, 2}, {3, 3} };

    const int8_t mode_2_counter[MB_MODE_COUNT] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 3, 1};

    const int8_t av1_mode_2_counter[AV1_MB_MODE_COUNT] = {
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,  // intra modes
        0, 0, 3, 1,                             // single ref modes
        0, 0, 1, 1, 1, 1, 3, 1                  // compound modes
    };

    typedef int8_t vpx_tree_index;
    struct av1_token {
        int32_t value;
        int32_t len;
    };

    const struct { uint8_t above, left; } partition_context_lookup[BLOCK_SIZES_ALL]= {
        { 31, 31 },  // 4X4     - {0b11111, 0b11111}
        { 31, 30 },  // 4X8     - {0b11111, 0b11110}
        { 30, 31 },  // 8X4     - {0b11110, 0b11111}
        { 30, 30 },  // 8X8     - {0b11110, 0b11110}
        { 30, 28 },  // 8X16    - {0b11110, 0b11100}
        { 28, 30 },  // 16X8    - {0b11100, 0b11110}
        { 28, 28 },  // 16X16   - {0b11100, 0b11100}
        { 28, 24 },  // 16X32   - {0b11100, 0b11000}
        { 24, 28 },  // 32X16   - {0b11000, 0b11100}
        { 24, 24 },  // 32X32   - {0b11000, 0b11000}
        { 24, 16 },  // 32X64   - {0b11000, 0b10000}
        { 16, 24 },  // 64X32   - {0b10000, 0b11000}
        { 16, 16 },  // 64X64   - {0b10000, 0b10000}
        { 16,  0 },  // 64X128  - {0b10000, 0b00000}
        {  0, 16 },  // 128X64  - {0b00000, 0b10000}
        {  0,  0 },  // 128X128 - {0b00000, 0b00000}
        { 31, 28 },  // 4X16    - {0b11111, 0b11100}
        { 28, 31 },  // 16X4    - {0b11100, 0b11111}
        { 30, 24 },  // 8X32    - {0b11110, 0b11000}
        { 24, 30 },  // 32X8    - {0b11000, 0b11110}
        { 28, 16 },  // 16X64   - {0b11100, 0b10000}
        { 16, 28 },  // 64X16   - {0b10000, 0b11100}
        { 24,  0 },  // 32X128  - {0b11000, 0b00000}
        {  0, 24 },  // 128X32  - {0b00000, 0b11000}
    };

#define TREE_SIZE(leaf_count) (2 * (leaf_count) - 2)
    const vpx_tree_index vp9_partition_tree[TREE_SIZE(PARTITION_TYPES)] = {
        -PARTITION_NONE, 2,
        -PARTITION_HORZ, 4,
        -PARTITION_VERT, -PARTITION_SPLIT
    };
    const vpx_tree_index av1_intra_mode_tree[TREE_SIZE(AV1_INTRA_MODES)] = {
        -DC_PRED,    2,            /* 0 = DC_NODE */
        -PAETH_PRED, 4,            /* 1 = PAETH_NODE */
        -V_PRED,     6,            /* 2 = V_NODE */
        8,           12,           /* 3 = COM_NODE */
        -H_PRED,     10,           /* 4 = H_NODE */
        -D135_PRED,  -D117_PRED,   /* 5 = D135_NODE */
        -D45_PRED,   14,           /* 6 = D45_NODE */
        -D63_PRED,   16,           /* 7 = D63_NODE */
        -D153_PRED,  18,           /* 8 = D153_NODE */
        -D207_PRED,  -SMOOTH_PRED, /* 9 = D207_NODE */
    };
    const vpx_tree_index vp9_token_tree[20] = {
        -ZERO_TOKEN, 2,
        -ONE_TOKEN, 4,
        6, 10,
        -TWO_TOKEN, 8,
        -THREE_TOKEN, -FOUR_TOKEN,
        12, 14,
        -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,
        16, 18,
        -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,
        -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6
    };
    const vpx_tree_index vp9_inter_mode_tree[6] = {
        -(ZEROMV - NEARESTMV), 2,
        -(NEARESTMV - NEARESTMV), 4,
        -(NEARMV - NEARESTMV), -(NEWMV - NEARESTMV)
    };
    const vpx_tree_index vp9_interp_filter_tree[4] = {
        -EIGHTTAP, 2,
        -EIGHTTAP_SMOOTH, -EIGHTTAP_SHARP
    };
    const vpx_tree_index vp9_mv_joint_tree[6] = {
        -MV_JOINT_ZERO, 2,
        -MV_JOINT_HNZVZ, 4,
        -MV_JOINT_HZVNZ, -MV_JOINT_HNZVNZ
    };
    const vpx_tree_index mv_class_tree[20] = {
        -MV_CLASS_0, 2,
        -MV_CLASS_1, 4,
        6, 8,
        -MV_CLASS_2, -MV_CLASS_3,
        10, 12,
        -MV_CLASS_4, -MV_CLASS_5,
        -MV_CLASS_6, 14,
        16, 18,
        -MV_CLASS_7, -MV_CLASS_8,
        -MV_CLASS_9, -MV_CLASS_10,
    };
    const vpx_tree_index av1_mv_fp_tree[TREE_SIZE(MV_FP_SIZE)] = { -0, 2,  -1,
        4,  -2, -3 };

    const vpx_tree_index av1_ext_tx_tree[TREE_SIZE(TX_TYPES)] = {
        -DCT_DCT, 2,
        -ADST_ADST, 4,
        -ADST_DCT, -DCT_ADST
    };

    const int av1_ext_tx_inv[EXT_TX_SET_TYPES][TX_TYPES] = {
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 9, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 9, 0, 10, 11, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 9, 10, 11, 0, 1, 2, 4, 5, 3, 6, 7, 8, 0, 0, 0, 0 },
            { 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 4, 5, 3, 6, 7, 8 },
    };

    const int av1_ext_tx_set_idx_to_type[2][4] = {
      {
            // Intra
            EXT_TX_SET_DCTONLY,
            EXT_TX_SET_DTT4_IDTX_1DDCT,
            EXT_TX_SET_DTT4_IDTX,
        },
        {
            // Inter
            EXT_TX_SET_DCTONLY,
            EXT_TX_SET_ALL16,
            EXT_TX_SET_DTT9_IDTX_1DDCT,
            EXT_TX_SET_DCT_IDTX,
        },
    };

    const int use_intra_ext_tx_for_txsize[EXT_TX_SETS_INTRA][EXT_TX_SIZES] = {
      { 1, 1, 1, 1 },  // unused
      { 1, 1, 0, 0 },
      { 0, 0, 1, 0 },
    };

    const uint8_t energy_class[12] = {0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5};

    enum { CAT1_MIN_VAL=5, CAT2_MIN_VAL=7, CAT3_MIN_VAL=11, CAT4_MIN_VAL=19, CAT5_MIN_VAL=35, CAT6_MIN_VAL=67 };

    const struct {int16_t cat, numExtra, coef; } extra_bits[11] = {
        { 0, 0, 0},
        { 0, 0, 1},
        { 0, 0, 2},
        { 0, 0, 3},
        { 0, 0, 4},
        { 1, 1, CAT1_MIN_VAL}, // 5..6
        { 2, 2, CAT2_MIN_VAL}, // 7..10
        { 3, 3, CAT3_MIN_VAL}, // 11..18
        { 4, 4, CAT4_MIN_VAL}, // 19..34
        { 5, 5, CAT5_MIN_VAL}, // 35..66
        { 6, 14, CAT6_MIN_VAL} // 67..
    };


    struct MvCompContextsVp9 {
        vpx_prob mv_sign;
        vpx_prob mv_class[AV1_MV_CLASSES - 1];
        vpx_prob mv_class0_bit[CLASS0_SIZE - 1];
        vpx_prob mv_bits[MV_OFFSET_BITS];
        vpx_prob mv_class0_fr[CLASS0_SIZE][MV_FP_SIZE - 1];
        vpx_prob mv_fr[MV_FP_SIZE - 1];
        vpx_prob mv_class0_hp;
        vpx_prob mv_hp;
    };

    struct MvContextsVp9 {
        vpx_prob mv_joint[MV_JOINTS - 1];
        MvCompContextsVp9 comps[2];
    };

    struct MvCompContextsAv1 {
        aom_cdf_prob classes_cdf[CDF_SIZE(AV1_MV_CLASSES)];
        aom_cdf_prob class0_fp_cdf[CLASS0_SIZE][CDF_SIZE(MV_FP_SIZE)];
        aom_cdf_prob fp_cdf[CDF_SIZE(MV_FP_SIZE)];
        aom_cdf_prob sign_cdf[CDF_SIZE(2)];
        aom_cdf_prob class0_hp_cdf[CDF_SIZE(2)];
        aom_cdf_prob hp_cdf[CDF_SIZE(2)];
        aom_cdf_prob class0_cdf[CDF_SIZE(CLASS0_SIZE)];
        aom_cdf_prob bits_cdf[MV_OFFSET_BITS][CDF_SIZE(2)];
    };

    struct MvContextsAv1 {
        aom_cdf_prob joint_cdf[CDF_SIZE(MV_JOINTS)];
        MvCompContextsAv1 comps[2];
    };

    struct FrameContexts {
        vpx_prob vp9_y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
        vpx_prob vp9_uv_mode[INTRA_MODES][INTRA_MODES - 1];
        vpx_prob partition[VP9_PARTITION_CONTEXTS][PARTITION_TYPES - 1];
        vpx_prob coef[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][UNCONSTRAINED_NODES];
        vpx_prob interp_filter[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1];
        vpx_prob inter_mode[INTER_MODE_CONTEXTS][INTER_MODES - 1];
        vpx_prob is_inter[INTRA_INTER_CONTEXTS];
        vpx_prob comp_mode[COMP_INTER_CONTEXTS];
        vpx_prob comp_ref_type[COMP_REF_TYPE_CONTEXTS];
        vpx_prob vp9_single_ref[VP9_REF_CONTEXTS][2];
        vpx_prob vp9_comp_ref[VP9_REF_CONTEXTS];
        vpx_prob tx[TX_SIZES][TX_SIZE_CONTEXTS][TX_SIZES-1];
        vpx_prob skip[SKIP_CONTEXTS];
        MvContextsVp9 mv_contexts;
    };

    struct ADzCtx {
        float aqroundFactorY[2];      // [DC/AC]
        float aqroundFactorUv[2][2];  // [U/v][DC/AC]
        float reserved[2];            // alignment
        void Add(float *dy, float duv[][2])
        {
            this->aqroundFactorY[0]     += dy[0];
            this->aqroundFactorY[1]     += dy[1];
            this->aqroundFactorUv[0][0] += duv[0][0];
            this->aqroundFactorUv[0][1] += duv[0][1];
            this->aqroundFactorUv[1][0] += duv[1][0];
            this->aqroundFactorUv[1][1] += duv[1][1];
        }
        void AddWieghted(float *dy, float duv[][2], float *y, float uv[][2])
        {
            const float dw[8] = { 1.0f, 0.984496437f, 0.939413063f, 0.868815056f, 0.778800783f, 0.676633846f, 0.569782825f, 0.465043188f };

            int di = Saturate(0, 7, (int)((this->aqroundFactorY[0] - y[0])*(this->aqroundFactorY[0] - y[0])*8.0f + 0.5f));
            this->aqroundFactorY[0] += dy[0] * dw[di];
            di = Saturate(0, 7, (int)((this->aqroundFactorY[1] - y[1])*(this->aqroundFactorY[1] - y[1])*8.0f + 0.5f));
            this->aqroundFactorY[1] += dy[1] * dw[di];
            di = Saturate(0, 7, (int)((this->aqroundFactorUv[0][0] - uv[0][0])*(this->aqroundFactorUv[0][0] - uv[0][0])*8.0f + 0.5f));
            this->aqroundFactorUv[0][0] += duv[0][0] * dw[di];
            di = Saturate(0, 7, (int)((this->aqroundFactorUv[0][1] - uv[0][1])*(this->aqroundFactorUv[0][1] - uv[0][1])*8.0f + 0.5f));
            this->aqroundFactorUv[0][1] += duv[0][1] * dw[di];
            di = Saturate(0, 7, (int)((this->aqroundFactorUv[1][0] - uv[1][0])*(this->aqroundFactorUv[1][0] - uv[1][0])*8.0f + 0.5f));
            this->aqroundFactorUv[1][0] += duv[1][0] * dw[di];
            di = Saturate(0, 7, (int)((this->aqroundFactorUv[1][1] - uv[1][1])*(this->aqroundFactorUv[1][1] - uv[1][1])*8.0f + 0.5f));
            this->aqroundFactorUv[1][1] += duv[1][1] * dw[di];
        }
        void SetMin(float *y, float uv[][2])
        {
            this->aqroundFactorY[0]     = MIN(this->aqroundFactorY[0], y[0]);
            this->aqroundFactorY[1]     = MIN(this->aqroundFactorY[1], y[1]);
            this->aqroundFactorUv[0][0] = MIN(this->aqroundFactorUv[0][0], uv[0][0]);
            this->aqroundFactorUv[0][1] = MIN(this->aqroundFactorUv[0][1], uv[0][1]);
            this->aqroundFactorUv[1][0] = MIN(this->aqroundFactorUv[1][0], uv[1][0]);
            this->aqroundFactorUv[1][1] = MIN(this->aqroundFactorUv[1][1], uv[1][1]);
        }
        void SetDcAc(float Dz0, float Dz1) {
            this->aqroundFactorY[0] = Dz0;
            this->aqroundFactorY[1] = Dz1;
            this->aqroundFactorUv[0][0] = this->aqroundFactorUv[1][0] = Dz0;
            this->aqroundFactorUv[0][1] = this->aqroundFactorUv[1][1] = Dz1;
        }
        void SaturateAcDc(float Dz0, float Dz1, float varDz)
        {
            this->aqroundFactorY[0]     = Saturate((Dz0 - varDz), (Dz0 + varDz), this->aqroundFactorY[0]);
            this->aqroundFactorUv[0][0] = Saturate((Dz0 - varDz), (Dz0 + varDz), this->aqroundFactorUv[0][0]);
            this->aqroundFactorUv[1][0] = Saturate((Dz0 - varDz), (Dz0 + varDz), this->aqroundFactorUv[1][0]);
            this->aqroundFactorY[1]     = Saturate((Dz1 - varDz), (Dz1 + varDz), this->aqroundFactorY[1]);
            this->aqroundFactorUv[0][1] = Saturate((Dz1 - varDz), (Dz1 + varDz), this->aqroundFactorUv[0][1]);
            this->aqroundFactorUv[1][1] = Saturate((Dz1 - varDz), (Dz1 + varDz), this->aqroundFactorUv[1][1]);
        }
    };

    enum {
        TWO_COLORS,
        THREE_COLORS,
        FOUR_COLORS,
        FIVE_COLORS,
        SIX_COLORS,
        SEVEN_COLORS,
        EIGHT_COLORS,
        PALETTE_SIZES
    }/* UENUM1BYTE(PALETTE_SIZE)*/;

    // Number of possible contexts for a color index.
// As can be seen from av1_get_palette_color_index_context(), the possible
// contexts are (2,0,0), (2,2,1), (3,2,0), (4,1,0), (5,0,0). These are mapped to
// a value from 0 to 4 using 'palette_color_index_context_lookup' table.
#define PALETTE_COLOR_INDEX_CONTEXTS 5

// Palette Y mode context for a block is determined by number of neighboring
// blocks (top and/or left) using a palette for Y plane. So, possible Y mode'
// context values are:
// 0 if neither left nor top block uses palette for Y plane,
// 1 if exactly one of left or top block uses palette for Y plane, and
// 2 if both left and top blocks use palette for Y plane.
#define PALETTE_Y_MODE_CONTEXTS 3

// Palette UV mode context for a block is determined by whether this block uses
// palette for the Y plane. So, possible values are:
// 0 if this block doesn't use palette for Y plane.
// 1 if this block uses palette for Y plane (i.e. Y palette size > 0).
#define PALETTE_UV_MODE_CONTEXTS 2

// Map the number of pixels in a block size to a context
//   64(BLOCK_8X8, BLOCK_4x16, BLOCK_16X4)  -> 0
//  128(BLOCK_8X16, BLOCK_16x8)             -> 1
//   ...
// 4096(BLOCK_64X64)                        -> 6
#define PALATTE_BSIZE_CTXS 7

    struct EntropyContexts {
        aom_cdf_prob skip_cdf[SKIP_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob single_ref_cdf[AV1_REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)];
        aom_cdf_prob comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob comp_ref_cdf[AV1_REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)];
        aom_cdf_prob comp_bwdref_cdf[AV1_REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)];
        aom_cdf_prob newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob txb_skip_cdf[TX_SIZES][TXB_SKIP_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob eob_extra_cdf[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob dc_sign_cdf[PLANE_TYPES][DC_SIGN_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob eob_flag_cdf16[PLANE_TYPES][2][CDF_SIZE(5)];
        aom_cdf_prob eob_flag_cdf32[PLANE_TYPES][2][CDF_SIZE(6)];
        aom_cdf_prob eob_flag_cdf64[PLANE_TYPES][2][CDF_SIZE(7)];
        aom_cdf_prob eob_flag_cdf128[PLANE_TYPES][2][CDF_SIZE(8)];
        aom_cdf_prob eob_flag_cdf256[PLANE_TYPES][2][CDF_SIZE(9)];
        aom_cdf_prob eob_flag_cdf512[PLANE_TYPES][2][CDF_SIZE(10)];
        aom_cdf_prob eob_flag_cdf1024[PLANE_TYPES][2][CDF_SIZE(11)];
        aom_cdf_prob coeff_base_eob_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB][CDF_SIZE(3)];
        aom_cdf_prob coeff_base_cdf[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS][CDF_SIZE(4)];
        aom_cdf_prob coeff_br_cdf[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS][CDF_SIZE(BR_CDF_SIZE)];
        aom_cdf_prob y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(AV1_INTRA_MODES)];
        aom_cdf_prob uv_mode_cdf[CFL_ALLOWED_TYPES][AV1_INTRA_MODES][CDF_SIZE(AV1_UV_INTRA_MODES)];
        aom_cdf_prob angle_delta_cdf[DIRECTIONAL_MODES][CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)];
        aom_cdf_prob filter_intra_cdfs[TX_SIZES_ALL][CDF_SIZE(2)];
        aom_cdf_prob partition_cdf[AV1_PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)];
        aom_cdf_prob switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(SWITCHABLE_FILTERS)];
        aom_cdf_prob tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH + 1)];
        aom_cdf_prob inter_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(INTER_MODES)];
        aom_cdf_prob inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(AV1_INTER_COMPOUND_MODES)];
        aom_cdf_prob kf_y_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][CDF_SIZE(AV1_INTRA_MODES)];
        aom_cdf_prob y_haspalette_cdf[PALETTE_BLOCK_SIZE_CONTEXTS][3][CDF_SIZE(2)];
        aom_cdf_prob y_palettesize_cdf[PALETTE_BLOCK_SIZE_CONTEXTS][CDF_SIZE(PALETTE_SIZES)];
        aom_cdf_prob uv_haspalette_cdf[2][CDF_SIZE(2)];
        aom_cdf_prob uv_palettesize_cdf[PALETTE_BLOCK_SIZE_CONTEXTS][CDF_SIZE(PALETTE_SIZES)];
        aom_cdf_prob y_palettecoloridx_cdf[PALETTE_SIZES][5][CDF_SIZE(MAX_PALETTE)];
        aom_cdf_prob uv_palette_color_index_cdf[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(MAX_PALETTE)];
        aom_cdf_prob intra_ext_tx_cdf[EXT_TX_SETS_INTRA][EXT_TX_SIZES][AV1_INTRA_MODES][CDF_SIZE(TX_TYPES)];
        aom_cdf_prob inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(TX_TYPES)];
        aom_cdf_prob motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)];
        aom_cdf_prob obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
        aom_cdf_prob txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)];
        aom_cdf_prob cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)];
        aom_cdf_prob cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)];
        aom_cdf_prob intrabc_cdf[CDF_SIZE(2)];
        aom_cdf_prob delta_q_abs_cdf[CDF_SIZE(DELTA_Q_SMALL + 1)];
        MvContextsAv1 mv_contexts;
        MvContextsAv1 dv_contexts;
    };


    const int32_t CONFIG_AV1_HIGHBITDEPTH = 0;
    const int32_t MINQ = 0;
    const int32_t MAXQ = 255;
    const int32_t QINDEX_RANGE = (MAXQ - MINQ + 1);
    const int32_t QINDEX_BITS = 8;

    struct QuantParam {
        int16_t zbin[2];
        int16_t round[2];
        int16_t quant[2];
        int16_t quantShift[2];
        int16_t dequant[2];
        int16_t quantFp[2];
        int16_t roundFp[2];
    };

    template <size_t N> struct As_ { uint8_t b[N]; };
    inline As_<16> & As16B(void *p) { assert((size_t(p) & 15) == 0); return *(As_<16>*)p; }
    inline As_<8>  & As8B(void *p)  { assert((size_t(p) & 7) == 0);  return *(As_<8>*)p; }
    inline As_<4>  & As4B(void *p)  { assert((size_t(p) & 3) == 0);  return *(As_<4>*)p; }
    inline As_<2>  & As2B(void *p)  { assert((size_t(p) & 1) == 0);  return *(As_<2>*)p; }
    inline const As_<16> & As16B(const void *p) { assert((size_t(p) & 15) == 0); return *(const As_<16>*)p; }
    inline const As_<8>  & As8B(const void *p)  { assert((size_t(p) & 7) == 0);  return *(const As_<8>*)p; }
    inline const As_<4>  & As4B(const void *p)  { assert((size_t(p) & 3) == 0);  return *(const As_<4>*)p; }
    inline const As_<2>  & As2B(const void *p)  { assert((size_t(p) & 1) == 0);  return *(const As_<2>*)p; }

    inline bool operator==(const As_<2> &l, const As_<2> &r) {
        return *reinterpret_cast<const uint16_t *>(&l) == *reinterpret_cast<const uint16_t *>(&r);
    }
    inline bool operator==(const As_<4> &l, const As_<4> &r) {
        return *reinterpret_cast<const uint32_t *>(&l) == *reinterpret_cast<const uint32_t *>(&r);
    }
    inline bool operator==(const As_<8> &l, const As_<8> &r) {
        return *reinterpret_cast<const uint64_t *>(&l) == *reinterpret_cast<const uint64_t *>(&r);
    }
    template <size_t N> inline bool operator!=(const As_<N> &l, const As_<N> &r) { return !(l == r); }

    union alignas(4) AV1MV {
        struct { int16_t mvx, mvy; };
        uint32_t asInt;
    };
    inline bool operator==(const AV1MV &l, const AV1MV &r) { return l.asInt == r.asInt; }
    inline bool operator!=(const AV1MV &l, const AV1MV &r) { return l.asInt != r.asInt; }
    constexpr AV1MV MV_ZERO = {};
    constexpr AV1MV INVALID_MV = { uint16_t(0x8000), uint16_t(0x8000) };

    struct alignas(8) AV1MVPair {
        AV1MV &operator[](int32_t i) { assert(i==0 || i==1); return mv[i]; }
        const AV1MV &operator[](int32_t i) const { assert(i==0 || i==1); return mv[i]; }
        operator AV1MV *() { return mv; }
        operator const AV1MV *() const { return mv; }
        union {
            AV1MV mv[2];
            uint64_t asInt64;
        };
    };
    inline bool operator==(const AV1MVPair &l, const AV1MVPair &r) { return l.asInt64 == r.asInt64; }
    inline bool operator!=(const AV1MVPair &l, const AV1MVPair &r) { return l.asInt64 != r.asInt64; }

    struct alignas(8) AV1MVCand {
        AV1MVPair mv;
        int32_t weight;
        union {
            uint16_t predDiff[2];
            uint32_t predDiffAsInt;
        };
    };

    class State {
        bool m_free; // resource in use or could be reused (m_free == true means could be reused).
    public:
        State() : m_free(true) { }
        bool IsFree() const { return m_free; }
        void SetFree(bool free) { m_free = free; }
    };

    enum EdgeDir { VERT_EDGE = 0, HORZ_EDGE = 1, NUM_EDGE_DIRS, ANY_EDGE = 0 };

    enum DeblockingLevelMethod
    {
        QPBASED,
        FULLSEARCH_FULLPIC,
        SMARTSEARCH_FULLPIC
    };

    inline BlockSize GetSbType(int32_t depth, PartitionType partition) {
        return BlockSize(3 * (4 - depth) - partition);
    }

#define permute4x64(r, m0, m1, m2, m3) _mm256_permute4x64_epi64(r, (m0) + ((m1) << 2) + ((m2) << 4) + ((m3) << 6))
#define permute2x128(r0, r1, m0, m1) _mm256_permute2x128_si256(r0, r1, (m0) + ((m1) << 4))
#define shuffle32(r, m0, m1, m2, m3) _mm256_shuffle_epi32(r, (m0) + ((m1) << 2) + ((m2) << 4) + ((m3) << 6));

    inline __m128i loada_si128(const void *p) {
        assert(!(size_t(p)&15));
        return _mm_load_si128(reinterpret_cast<const __m128i *>(p));
    }

    inline __m256i loada_si256(const void *p) {
        assert(!(size_t(p) & 31));
        return _mm256_load_si256(reinterpret_cast<const __m256i *>(p));
    }

    inline __m256i si256(__m128i r) { return _mm256_castsi128_si256(r); }
    inline __m128i si128(__m256i r) { return _mm256_castsi256_si128(r); }
    inline __m128i si128_lo(__m256i r) { return si128(r); }
    inline __m128i si128_hi(__m256i r) { return _mm256_extracti128_si256(r, 1); }

    inline __m256i loada2_m128i(const void *lo, const void *hi) {
        return _mm256_insertf128_si256(si256(loada_si128(lo)), loada_si128(hi), 1);
    }

    inline __m128i loadu_si128(const void *p) {
        return _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
    }

    inline __m256i loadu_si256(const void *p) {
        return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
    }

    inline void storea_si128(void *p, __m128i r) {
        assert(!(size_t(p)&15));
        return _mm_store_si128(reinterpret_cast<__m128i *>(p), r);
    }

    inline void storea_si256(void *p, __m256i r) {
        assert(!(size_t(p) & 31));
        return _mm256_store_si256(reinterpret_cast<__m256i *>(p), r);
    }

    inline void storeu_si256(void *p, __m256i r) {
        return _mm256_storeu_si256(reinterpret_cast<__m256i *>(p), r);
    }

    inline void storea2_m128i(void *lo, void *hi, __m256i r) {
        storea_si128(lo, si128(r)); storea_si128(hi, _mm256_extracti128_si256(r, 1));
    }

    inline void storeu_si128(void *p, __m128i r) {
        return _mm_storeu_si128(reinterpret_cast<__m128i *>(p), r);
    }

    inline __m128i loadl_epi64(const void *p) {
        return _mm_loadl_epi64((__m128i*)p);
    }

    inline __m128i loadh_epi64(__m128i r, const void *p) {
        return _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(r), (const double *)p));
    }

    inline __m128i loadh_epi64(const void *p) {
        return loadh_epi64(_mm_setzero_si128(), p);
    }

    inline __m256i loadu2_m128i(const void *lo, const void *hi) {
        return _mm256_loadu2_m128i((const __m128i *)hi, (const __m128i *)lo);
    }

    inline void storeu2_m128i(void *lo, void *hi, __m256i r) {
        _mm256_storeu2_m128i((__m128i *)hi, (__m128i *)lo, r);
    }

    inline __m128i loadu2_epi64(const void *lo, const void *hi) {
        return loadh_epi64(loadl_epi64(lo), hi);
    }

    inline __m128i loadu_si32(const void *p) {
        assert(!(size_t(p)&3));
        return _mm_castps_si128(_mm_load_ss((const float*)p));
    }

    inline __m128i loadu4_epi32(const void *p0, const void *p1, const void *p2, const void *p3) {
        return _mm_setr_epi32(*(const int *)p0, *(const int *)p1, *(const int *)p2, *(const int *)p3);
    }

    inline void storel_epi64(void *p, __m128i r) {
        _mm_storel_epi64((__m128i *)p, r);
    }

    inline void storeh_epi64(void *p, __m128i r) {
        _mm_storeh_pd((double*)p, _mm_castsi128_pd(r));
    }

    inline void storel_si32(void *p, __m128i r) {
        _mm_store_ss((float *)p, _mm_castsi128_ps(r));
    }

    inline void Copy64(const void *src, void *dst) {
        assert(!(size_t(src)&7));
        assert(!(size_t(dst)&7));
        *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<const uint64_t*>(src);
    }

    inline void Copy32(const void *src, void *dst) {
        assert(!(size_t(src)&3));
        assert(!(size_t(dst)&3));
        *reinterpret_cast<uint32_t*>(dst) = *reinterpret_cast<const uint32_t*>(src);
    }

    enum EnumCodecType { CODEC_AV1, CODEC_DEFAULT = CODEC_AV1};

    template <int32_t zeroOrOne, int32_t nbytes> inline void small_memset_impl(void *ptr);
    template <> inline void small_memset_impl<0,1>(void *ptr)  { reinterpret_cast<uint8_t *>(ptr)[0]  = 0; }
    template <> inline void small_memset_impl<0,2>(void *ptr)  { reinterpret_cast<uint16_t *>(ptr)[0] = 0; }
    template <> inline void small_memset_impl<0,4>(void *ptr)  { reinterpret_cast<uint32_t *>(ptr)[0] = 0; }
    template <> inline void small_memset_impl<0,8>(void *ptr)  { reinterpret_cast<uint64_t *>(ptr)[0] = 0; }
    template <> inline void small_memset_impl<0,16>(void *ptr) { storea_si128(ptr, _mm_setzero_si128()); }
    template <> inline void small_memset_impl<1,1>(void *ptr)  { reinterpret_cast<uint8_t *>(ptr)[0]  = 0x1; }
    template <> inline void small_memset_impl<1,2>(void *ptr)  { reinterpret_cast<uint16_t *>(ptr)[0] = 0x101; }
    template <> inline void small_memset_impl<1,4>(void *ptr)  { reinterpret_cast<uint32_t *>(ptr)[0] = 0x1010101; }
    template <> inline void small_memset_impl<1,8>(void *ptr)  { reinterpret_cast<uint64_t *>(ptr)[0] = 0x101010101010101; }
    template <> inline void small_memset_impl<1,16>(void *ptr) {
        __m128i ones = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
        ones = _mm_srli_epi16(ones, 15);
        ones = _mm_packus_epi16(ones, ones);
        storea_si128(ptr, ones);
    }

    inline void small_memset0(void *ptr, int32_t nbytes) {
        assert(nbytes == 1 || nbytes == 2 || nbytes == 4 || nbytes == 8 || nbytes == 16);
        switch (nbytes) {
        default: assert(0);
        case 1:  return small_memset_impl<0,1>(ptr);
        case 2:  return small_memset_impl<0,2>(ptr);
        case 4:  return small_memset_impl<0,4>(ptr);
        case 8:  return small_memset_impl<0,8>(ptr);
        case 16: return small_memset_impl<0,16>(ptr);
        }
    }
    inline void small_memset1(void *ptr, int32_t nbytes) {
        assert(nbytes == 1 || nbytes == 2 || nbytes == 4 || nbytes == 8 || nbytes == 16);
        switch (nbytes) {
        default: assert(0);
        case 1:  return small_memset_impl<1,1>(ptr);
        case 2:  return small_memset_impl<1,2>(ptr);
        case 4:  return small_memset_impl<1,4>(ptr);
        case 8:  return small_memset_impl<1,8>(ptr);
        case 16: return small_memset_impl<1,16>(ptr);
        }
    }
    inline void SetCulLevel(uint8_t *actx, uint8_t *lctx, uint8_t culLevel, TxSize txSize)
    {
        if (txSize == TX_4X4) {
            *actx = *lctx = culLevel;
        } else if (txSize == TX_8X8) {
            *(uint16_t *)actx = *(uint16_t *)lctx = culLevel + (culLevel << 8);
        } else if (txSize == TX_16X16) {
            uint32_t tmp = culLevel + (culLevel << 8);
            *(uint32_t *)actx = *(uint32_t *)lctx = tmp + (tmp << 16);
        } else if (txSize == TX_32X32) {
            uint64_t tmp = culLevel + (culLevel << 8);
            tmp = tmp + (tmp << 16);
            *(uint64_t *)actx = *(uint64_t *)lctx = tmp + (tmp << 32);
        } else {
            assert(0);
        }
    }

    const int32_t MODE_CTX_REF_FRAMES = (TOTAL_REFS_PER_FRAME + COMP_REFS);
    const int32_t MAX_REF_MV_STACK_SIZE = 8;

    const int32_t REF_CAT_LEVEL = 640;
    const int32_t MAX_MIB_MASK = 7;
    const int32_t MAX_MIB_SIZE = 8;
    const int32_t SAMPLES_ARRAY_SIZE = (MAX_MIB_SIZE * 2 + 2) * 2;
    const int32_t MAX_MIB_SIZE_LOG2 = 3;

    const int32_t FRAME_ID_NUMBERS_PRESENT_FLAG = 1;
    const int32_t FRAME_ID_LENGTH_MINUS7 = 8;
    const int32_t DELTA_FRAME_ID_LENGTH_MINUS2 = 12;

    inline PartitionType GetPartition(BlockSize currBlockSize, BlockSize targetBlockSize) {
        const int32_t currWide = block_size_wide[currBlockSize];
        const int32_t currHigh = block_size_high[currBlockSize];
        const int32_t targetWide = block_size_wide[targetBlockSize];
        const int32_t targetHigh = block_size_wide[targetBlockSize];
        assert(currWide >= targetWide);
        assert(currHigh >= targetHigh);
        if (currWide == targetWide) assert(currHigh == targetHigh || currHigh == 2 * targetHigh);
        if (currHigh == targetHigh) assert(currWide == targetWide || currWide == 2 * targetWide);
        const int32_t vertSplit = targetHigh < currHigh;
        const int32_t horzSplit = targetWide < currWide;
        return PartitionType(horzSplit + (vertSplit << 1));
    }

    struct TileBorders {
        int32_t colStart;
        int32_t colEnd;
        int32_t rowStart;
        int32_t rowEnd;
    };

    inline aom_cdf_prob cdf_element_prob(const aom_cdf_prob *cdf, size_t element) {
        return (element > 0 ? cdf[element - 1] : CDF_PROB_TOP) - cdf[element];
    }

    inline void partition_gather_horz_alike(aom_cdf_prob *out, const aom_cdf_prob *in, BlockSize bsize) {
        out[0] = CDF_PROB_TOP;
        out[0] -= cdf_element_prob(in, PARTITION_HORZ);
        out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
        out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
        out[0] -= cdf_element_prob(in, PARTITION_HORZ_B);
        out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
        if (bsize != BLOCK_128X128)
            out[0] -= cdf_element_prob(in, PARTITION_HORZ_4);
        out[0] = AOM_ICDF(out[0]);
        out[1] = AOM_ICDF(CDF_PROB_TOP);
    }

    inline void partition_gather_vert_alike(aom_cdf_prob *out, const aom_cdf_prob *in, BlockSize bsize) {
        out[0] = CDF_PROB_TOP;
        out[0] -= cdf_element_prob(in, PARTITION_VERT);
        out[0] -= cdf_element_prob(in, PARTITION_SPLIT);
        out[0] -= cdf_element_prob(in, PARTITION_HORZ_A);
        out[0] -= cdf_element_prob(in, PARTITION_VERT_A);
        out[0] -= cdf_element_prob(in, PARTITION_VERT_B);
        if (bsize != BLOCK_128X128)
            out[0] -= cdf_element_prob(in, PARTITION_VERT_4);
        out[0] = AOM_ICDF(out[0]);
        out[1] = AOM_ICDF(CDF_PROB_TOP);
    }

    TxSetType get_ext_tx_set_type(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set);
    int32_t get_ext_tx_set(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set);
    int32_t get_ext_tx_types(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set);

    struct ScanOrder {
        const int16_t *scan;
        const int16_t *neighbors;
    };

    const int32_t MFMV_STACK_SIZE = 3;
    struct TplMvRef {
        AV1MV mfmv0[MFMV_STACK_SIZE];
        uint8_t ref_frame_offset[MFMV_STACK_SIZE];
    };

    struct CFL_params {
        uint8_t alpha_u : 4;
        uint8_t alpha_v : 4;
        uint8_t joint_sign : 3;
    };

    inline int32_t GetRelativeDist(int32_t bits, int32_t a, int32_t b)
    {
        if (bits == 0)
            return 0;
        assert(bits >= 1);
        assert(a >= 0 && a < (1 << bits));
        assert(b >= 0 && b < (1 << bits));
        const int32_t m = 1 << (bits - 1);
        const int32_t diff = a - b;
        return (diff & (m - 1)) - (diff & m);
    }

    struct TileParam {
        int32_t uniformSpacing;
        int32_t rows;
        int32_t cols;
        int32_t numTiles;
        int32_t log2Rows;
        int32_t log2Cols;
        int32_t minLog2Cols;
        int32_t minLog2Rows;
        int32_t maxLog2Cols;
        int32_t maxLog2Rows;
        int32_t minLog2Tiles;
        int32_t maxTileWidthSb;
        int32_t maxTileHeightSb;

        uint32_t mapSb2TileRow[(CodecLimits::MAX_HEIGHT + 63) / 64];
        uint32_t mapSb2TileCol[(CodecLimits::MAX_WIDTH + 63) / 64];

        uint16_t colStart[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t colEnd[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t colWidth[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t rowStart[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t rowEnd[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t rowHeight[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miColStart[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miColEnd[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miColWidth[CodecLimits::AV1_MAX_NUM_TILE_COLS];
        uint16_t miRowStart[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miRowEnd[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
        uint16_t miRowHeight[CodecLimits::AV1_MAX_NUM_TILE_ROWS];
    };

    inline int32_t DivUp(int32_t n, int32_t d) { return (n + d - 1) / d; }
    const int32_t MAX_NUM_GPU_SLICES = 8;
};

#endif // __MFX_AV1_DEFS_H__

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
