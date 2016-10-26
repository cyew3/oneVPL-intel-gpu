//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_PAK_UTILS_H_
#define _MFX_H264_PAK_UTILS_H_

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include <assert.h>
#include "mfxdefs.h"
#include "vm_interlocked.h"
#include "umc_thread.h"
#include "umc_event.h"

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define H264PAK_ALIGN16 __declspec (align(16))
#elif defined(__GNUC__)
#define H264PAK_ALIGN16 __attribute__ ((aligned (16)))
#else
#define H264PAK_ALIGN16
#endif

enum
{
    MFX_PAYLOAD_LAYOUT_ALL  = 0,

    // Mediasdk-man: "for field pictures,
    // odd payloads are associated with the first field and
    // even payloads are associated with the second field".
    MFX_PAYLOAD_LAYOUT_EVEN = 1,
    MFX_PAYLOAD_LAYOUT_ODD  = 2
};

enum
{
    // General sei message
    // ExtBuffer contains one or more sei messages
    // correctly formatted and ready to be put in a bitstream
    MFX_CUC_AVC_SEI_MESSAGES         = MFX_MAKEFOURCC('S','E','I','X'),

    // Special types of sei messages
    MFX_CUC_AVC_SEI_BUFFERING_PERIOD = MFX_MAKEFOURCC('S','E','I','0'),
    MFX_CUC_AVC_SEI_PIC_TIMING       = MFX_MAKEFOURCC('S','E','I','1')
};

struct mfxExtAvcSeiMessage
{
    mfxExtBuffer Header;
    mfxU16 numPayload;
    mfxU16 layout;
    mfxPayload** payload;
};

struct mfxExtAvcSeiBufferingPeriod
{
    mfxExtBuffer Header;

    mfxU8  seq_parameter_set_id;
    mfxU8  nal_cpb_cnt;
    mfxU8  vcl_cpb_cnt;
    mfxU8  initial_cpb_removal_delay_length;
    mfxU32 nal_initial_cpb_removal_delay[32];
    mfxU32 nal_initial_cpb_removal_delay_offset[32];
    mfxU32 vcl_initial_cpb_removal_delay[32];
    mfxU32 vcl_initial_cpb_removal_delay_offset[32];
};

struct mfxExtAvcSeiPicTiming
{
    mfxExtBuffer Header;

    mfxU8  cpb_dpb_delays_present_flag;
    mfxU8  cpb_removal_delay_length;
    mfxU8  dpb_output_delay_length;
    mfxU8  pic_struct_present_flag;
    mfxU8  time_offset_length;

    mfxU32 cpb_removal_delay;
    mfxU32 dpb_output_delay;
    mfxU8  pic_struct;
    mfxU8  ct_type;
};

struct mfxExtAvcSeiDecRefPicMrkRep
{
    mfxExtBuffer Header;
    
    mfxU8   original_idr_flag;
    mfxU16  original_frame_num;         // theoretical max frame_num is 2^16 - 1
    mfxU8   original_field_info_present_flag;
    mfxU8   original_field_pic_flag;
    mfxU8   original_bottom_field_flag;
    mfxU8 * dec_ref_pic_marking;        // FIXME: structured dec_ref_pic_marking here instead of packed form
    mfxU8   completedBytes;             // number of completed bytes in dec_ref_pic_marking() syntax
    mfxU8   bitOffset;                  // bit offset in last byte 
};

namespace H264Pak
{
    enum
    {
        NUM_MV_PER_MB = 32
    };

    enum
    {
        BLOCK_DC_Y = 0,
        BLOCK_DC_U = 1,
        BLOCK_DC_V = 2,
    };

    enum
    {
        SUB_MB_PRED_MODE_L0     = 0,
        SUB_MB_PRED_MODE_L1     = 1,
        SUB_MB_PRED_MODE_Bi     = 2,
    };

    enum
    {
        INTER_MB_MODE_16x16         = 0,
        INTER_MB_MODE_16x8          = 1,
        INTER_MB_MODE_8x16          = 2,
        INTER_MB_MODE_8x8           = 3,
    };

    enum
    {
        SUB_MB_SHAPE_8x8    = 0,
        SUB_MB_SHAPE_8x4    = 1,
        SUB_MB_SHAPE_4x8    = 2,
        SUB_MB_SHAPE_4x4    = 3,
    };

    enum
    {
        MBTYPE_I_4x4            = 0,
        MBTYPE_I_8x8            = 0,
        MBTYPE_I_16x16_000      = 1,
        MBTYPE_I_16x16_100      = 2,
        MBTYPE_I_16x16_200      = 3,
        MBTYPE_I_16x16_300      = 4,
        MBTYPE_I_16x16_010      = 5,
        MBTYPE_I_16x16_110      = 6,
        MBTYPE_I_16x16_210      = 7,
        MBTYPE_I_16x16_310      = 8,
        MBTYPE_I_16x16_020      = 0,
        MBTYPE_I_16x16_120      = 0xA,
        MBTYPE_I_16x16_220      = 0xB,
        MBTYPE_I_16x16_320      = 0xC,
        MBTYPE_I_16x16_001      = 0xD,
        MBTYPE_I_16x16_101      = 0xE,
        MBTYPE_I_16x16_201      = 0xF,
        MBTYPE_I_16x16_301      = 0x10,
        MBTYPE_I_16x16_011      = 0x11,
        MBTYPE_I_16x16_111      = 0x12,
        MBTYPE_I_16x16_211      = 0x13,
        MBTYPE_I_16x16_311      = 0x14,
        MBTYPE_I_16x16_021      = 0x15,
        MBTYPE_I_16x16_121      = 0x16,
        MBTYPE_I_16x16_221      = 0x17,
        MBTYPE_I_16x16_321      = 0x18,
        MBTYPE_I_IPCM           = 0x19,
    };

    enum
    {
        MBTYPE_BP_L0_16x16      = 1,
        MBTYPE_B_L1_16x16       = 2,
        MBTYPE_B_Bi_16x16       = 3,
        MBTYPE_BP_L0_L0_16x8    = 4,
        MBTYPE_BP_L0_L0_8x16    = 5,
        MBTYPE_B_L1_L1_16x8     = 6,
        MBTYPE_B_L1_L1_8x16     = 7,
        MBTYPE_B_L0_L1_16x8     = 8,
        MBTYPE_B_L0_L1_8x16     = 9,
        MBTYPE_B_L1_L0_16x8     = 0xA,
        MBTYPE_B_L1_L0_8x16     = 0xB,
        MBTYPE_B_L0_Bi_16x8     = 0xC,
        MBTYPE_B_L0_Bi_8x16     = 0xD,
        MBTYPE_B_L1_Bi_16x8     = 0xE,
        MBTYPE_B_L1_Bi_8x16     = 0xF,
        MBTYPE_B_Bi_L0_16x8     = 0x10,
        MBTYPE_B_Bi_L0_8x16     = 0x11,
        MBTYPE_B_Bi_L1_16x8     = 0x12,
        MBTYPE_B_Bi_L1_8x16     = 0x13,
        MBTYPE_B_Bi_Bi_16x8     = 0x14,
        MBTYPE_B_Bi_Bi_8x16     = 0x15,
        MBTYPE_BP_8x8           = 0x16,
    };

    static const mfxI32 g_ScanMatrixDec2x2[4] =
    {
        0,1,2,3
    };

    static const mfxI32 g_ScanMatrixDec4x4[2][16] =
    {
        {0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15},
        {0,4,1,8,12,5,9,13,2,6,10,14,3,7,11,15}
    };

    static const mfxI32 g_ScanMatrixDec8x8[2][64] =
    {
        {
             0,  1,  8, 16,  9,  2,  3, 10,
            17, 24, 32, 25, 18, 11,  4,  5,
            12, 19, 26, 33, 40, 48, 41, 34,
            27, 20, 13,  6,  7, 14, 21, 28,
            35, 42, 49, 56, 57, 50, 43, 36,
            29, 22, 15, 23, 30, 37, 44, 51,
            58, 59, 52, 45, 38, 31, 39, 46,
            53, 60, 61, 54, 47, 55, 62, 63
        },
        {
             0,  8, 16,  1,  9, 24, 32, 17,
             2, 25, 40, 48, 56, 33, 10,  3,
            18, 41, 49, 57, 26, 11,  4, 19,
            34, 42, 50, 58, 27, 12,  5, 20,
            35, 43, 51, 59, 28, 13,  6, 21,
            36, 44, 52, 60, 29, 14, 22, 37,
            45, 53, 61, 30,  7, 15, 38, 46,
            54, 62, 23, 31, 39, 47, 55, 63
        }
    };

    struct CavlcData
    {
        mfxI16 coeffs[16];
    };

    struct CabacData4x4
    {
        mfxI16 coeffs[16];
    };

    struct CabacData8x8
    {
        mfxI16 coeffs[64];
    };

    struct CavlcDataAux
    {
        mfxU8 lastSigCoeff;
    };

    struct CabacDataAux
    {
        mfxU8 numSigCoeff;
    };

    #pragma warning(disable : 4324)
    union H264PAK_ALIGN16 CoeffData
    {
        struct Cavlc
        {
            CavlcData luma[16];
            CavlcData dc[3];
            CavlcData chroma[8];
            CavlcDataAux lumaAux[16];
            CavlcDataAux dcAux[3];
            CavlcDataAux chromaAux[8];
        } cavlc;

        struct Cabac
        {
            union
            {
                CabacData4x4 luma4x4[16];
                CabacData8x8 luma8x8[4];
            };

            CabacData4x4 dc[3];
            CabacData4x4 chroma[8];

            union
            {
                CabacDataAux luma4x4Aux[16];
                CabacDataAux luma8x8Aux[4];
            };

            CabacDataAux dcAux[3];
            CabacDataAux chromaAux[8];
        } cabac;
    };
    #pragma warning(default : 4324)

    struct MbMvs
    {
        mfxI16Pair mv[2][16];
    };

    struct Block4x4
    {
        Block4x4(mfxU16 block_, mfxU16 sliceId_, mfxMbCodeAVC* mb_, MbMvs* mv_, MbMvs* mvd_)
            : block(block_)
            , sliceId(sliceId_)
            , mb(mb_)
            , mv(mv_)
            , mvd(mvd_)
        {
        }

        mfxU16 block;
        mfxU16 sliceId;
        mfxMbCodeAVC* mb;
        MbMvs* mv;
        MbMvs* mvd;
    };

    struct GeometryBlock4x4
    {
        mfxU8 offBlk4x4; // number of 4x4 block in raster scan
        mfxU8 xBlk;
        mfxU8 yBlk;
        mfxU8 offBlk8x8; // number of 8x8 block which 4x4 block belongs to
    };

    static const GeometryBlock4x4 g_Geometry[16] =
    {
        { 0,  0,  0,  0 },
        { 1,  1,  0,  0 },
        { 4,  0,  1,  0 },
        { 5,  1,  1,  0 },
        { 2,  2,  0,  1 },
        { 3,  3,  0,  1 },
        { 6,  2,  1,  1 },
        { 7,  3,  1,  1 },
        { 8,  0,  2,  2 },
        { 9,  1,  2,  2 },
        {12,  0,  3,  2 },
        {13,  1,  3,  2 },
        {10,  2,  2,  3 },
        {11,  3,  2,  3 },
        {14,  2,  3,  3 },
        {15,  3,  3,  3 }
    };

    static const mfxU8 g_MapLumaQpToChromaQp[52] =
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
        26, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 34, 35,
        35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39
    };

    /* left 4x4 block:
     *
     *  0  1  4  5  ->  5  0  1  4
     *  2  3  6  7  ->  7  2  3  6
     *  8  9 12 13  -> 13  8  9 12
     * 10 11 14 15  -> 15 10 11 14
     */
    static const mfxU8 g_BlockA[16] =
    {
        5, 0, 7, 2, 1, 4, 3, 6, 13, 8, 15, 10, 9, 12, 11, 14
    };

    /* above 4x4 block:
     *
     *  0  1  4  5  -> 10 11 14 15
     *  2  3  6  7  ->  0  1  4  5
     *  8  9 12 13  ->  2  3  6  7
     * 10 11 14 15  ->  8  9 12 13
     */
    static const mfxU8 g_BlockB[16] =
    {
        10, 11, 0, 1, 14, 15, 4, 5, 2, 3, 8, 9, 6, 7, 12, 13
    };

    /* above right 4x4 block:
     *
     *  0  1  4  5  -> 11 14 15 10
     *  2  3  6  7  ->  1  4  5  0
     *  8  9 12 13  ->  3  6  7  2
     * 10 11 14 15  ->  9 12 13  8
     */
    static const mfxU8 g_BlockC[16] =
    {
        11, 14, 1, 4, 15, 10, 5, 0, 3, 6, 9, 12, 7, 2, 13, 8
    };

    /* above left 4x4 block:
     *
     *  0  1  4  5  -> 15 10 11 14
     *  2  3  6  7  ->  5  0  1  4
     *  8  9 12 13  ->  7  2  3  6
     * 10 11 14 15  -> 13  8  9 12
     */
    static const mfxU8 g_BlockD[16] =
    {
        15, 10, 5, 0, 11, 14, 1, 4, 7, 2, 13, 8, 3, 6, 9, 12
    };

    static const mfxI16Pair nullMv = { 0, 0 };

    struct SeqScalingMatrices8x8
    {
        mfxI16 m_seqScalingMatrix8x8[2][6][64];
        mfxI16 m_seqScalingMatrix8x8Inv[2][6][64];
    };

    struct PakResources
    {
        PakResources() { memset(this, 0, sizeof(*this)); }

        mfxMemId m_resId;
        mfxU8* m_mvdBuffer; // needed only for CABAC
        mfxU8* m_bsBuffer; // FIXME: try to remove
        mfxU8* m_coeffs; // FIXME: try to reduce
        mfxU8* m_mbDone;
        mfxU32 m_mvdBufferSize;
        mfxU32 m_bsBufferSize;
        mfxU32 m_coeffsSize;
        mfxU32 m_mbDoneSize;
    };

    struct CavlcCoeffInfo
    {
        mfxU8 luma[16];
        mfxU8 chroma[8];
    };

    inline mfxU8 GetNumCavlcCoeffLuma(const mfxMbCodeAVC& mb, mfxU32 block)
    {
        assert(block < 16);
        return ((CavlcCoeffInfo *)mb.reserved1a)->luma[block];
    }

    inline mfxU8 GetNumCavlcCoeffChroma(const mfxMbCodeAVC& mb, mfxU32 block)
    {
        assert(block < 8);
        return ((CavlcCoeffInfo *)mb.reserved1a)->chroma[block];
    }

    inline void SetNumCavlcCoeffLuma(const mfxMbCodeAVC& mb, mfxU32 block, mfxU8 numCoeff)
    {
        assert(block < 16);
        assert(numCoeff <= 16);
        ((CavlcCoeffInfo *)mb.reserved1a)->luma[block] = numCoeff;
    }

    inline void SetNumCavlcCoeffChroma(const mfxMbCodeAVC& mb, mfxU32 block, mfxU8 numCoeff)
    {
        assert(block < 8);
        assert(numCoeff <= 16);
        ((CavlcCoeffInfo *)mb.reserved1a)->chroma[block] = numCoeff;
    }

    template<class T> inline T AlignValue(T value, size_t alignment)
    {
        assert((alignment & (alignment - 1)) == 0); // should be 2^n
        return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
    }

    template<class T> inline T* AlignValue(T* value, size_t alignment)
    {
        return reinterpret_cast<T*>(AlignValue<size_t>(reinterpret_cast<size_t>(value), alignment));
    }

    inline mfxExtBuffer* GetExtBuffer(mfxExtBuffer** extBuffer, mfxU32 numExtBuffer, mfxU32 id)
    {
        for (mfxU32 i = 0; i < numExtBuffer; i++)
        {
            if (extBuffer[i] != 0 && extBuffer[i]->BufferId == id)
            {
                return extBuffer[i];
            }
        }

        return 0;
    }

    inline mfxExtBuffer* GetExtBuffer(const mfxFrameCUC& cuc, mfxU32 id)
    {
        return GetExtBuffer(cuc.ExtBuffer, cuc.NumExtBuffer, id);
    }

    inline const mfxExtCodingOption& GetExtCodingOption(const mfxVideoParam& vp)
    {
        // MFX_EXTBUFF_CODING_OPTION must present
        mfxExtBuffer* buf = GetExtBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_CODING_OPTION);
        assert(buf);
        return *(const mfxExtCodingOption *)buf;
    }

    inline mfxU8 GetMfxOpt(mfxU32 mfxOpt, mfxU8 defaultValue)
    {
        switch (mfxOpt)
        {
        case MFX_CODINGOPTION_ON:
            return 1;
        case MFX_CODINGOPTION_OFF:
            return 0;
        case MFX_CODINGOPTION_UNKNOWN:
            return defaultValue;
        default:
            assert(!"bad extended option");
            return defaultValue;
        }
    }

    inline mfxU8 GetMfxOptDefaultOn(mfxU32 mfxOpt)
    {
        return GetMfxOpt(mfxOpt, 1);
    }

    inline mfxU8 GetMfxOptDefaultOff(mfxU32 mfxOpt)
    {
        return GetMfxOpt(mfxOpt, 0);
    }

    inline bool operator==(const mfxI16Pair& l, const mfxI16Pair& r)
    {
        return l.x == r.x && l.y == r.y;
    }

    inline mfxU8 Get4x4IntraMode(const mfxMbCodeAVC& mb, mfxU32 block)
    {
        assert(block < 16);
        return (mfxU8)((mb.LumaIntraModes[block / 4] >> (4 * (block % 4))) & 0xF);
    }

    inline mfxU8 Get8x8IntraMode(const mfxMbCodeAVC& mb, mfxU32 block)
    {
        assert(block < 4);
        return (mfxU8)((mb.LumaIntraModes[0] >> (4 * block)) & 0xF);
    }

    inline mfxU8 Get16x16IntraMode(const mfxMbCodeAVC& mb)
    {
        return (mfxU8)(mb.LumaIntraModes[0] & 0x3);
    }

    inline bool IsOnLeftEdge(mfxU32 block)
    {
        return (block & 5) == 0;
    }

    inline bool IsOnTopEdge(mfxU32 block)
    {
        return (block & 10) == 0;
    }

    inline bool IsOnRightEdge(mfxU32 block)
    {
        return (block & 5) == 5;
    }

    inline mfxU32 GetMbCnt(const mfxFrameParamAVC& fp, const mfxMbCodeAVC& mb)
    {
        return mb.MbXcnt + mb.MbYcnt * (fp.FrameWinMbMinus1 + 1);
    }

    inline bool IsTrueIntra(const mfxMbCodeAVC& mb)
    {
        return mb.IntraMbFlag && mb.MbType5Bits != MBTYPE_I_IPCM;
    }

    inline bool IsIntra16x16(const mfxMbCodeAVC& mb)
    {
        return mb.IntraMbFlag && mb.MbType5Bits >= MBTYPE_I_16x16_000 && mb.MbType5Bits <= MBTYPE_I_16x16_321;
    }

    inline mfxU32 CeilLog2(mfxU32 val)
    {
        mfxU32 res = 0;

        while (val)
        {
            val >>= 1;
            res++;
        }

        return res;
    }

    struct Partition
    {
        Partition() {}

        Partition(mfxU32 b, mfxU8 w, mfxU8 h)
            : block((mfxU8)b), width(w), height(h)
        {
            assert(block < 16);
        }

        mfxU8 block;
        mfxU8 width;
        mfxU8 height;
    };

    class MbPartitionIterator
    {
    public:
        explicit MbPartitionIterator(const mfxMbCodeAVC& mb);

        Partition parts[16];
        mfxU32 m_numPart;
        bool noPartsLessThan8x8;
    };

    class MbDescriptor
    {
    public:
        MbDescriptor(const mfxFrameCUC& cuc, mfxU8* mvdBuffer, mfxU32 mbNum, bool ignoreSliceBoundaries = false);

        Block4x4 GetCurrent(mfxU32 block) const
        {
            assert(block < 16);
            return Block4x4((mfxU16)block, sliceIdN, &mbN, mvN, mvdN);
        }

        Block4x4 GetLeft(mfxU32 block) const
        {
            assert(block < 16);
            return IsOnLeftEdge(block)
                ? Block4x4(g_BlockA[block], sliceIdA, mbA, mvA, mvdA)
                : Block4x4(g_BlockA[block], sliceIdN, &mbN, mvN, mvdN);
        }

        Block4x4 GetAbove(mfxU32 block) const
        {
            assert(block < 16);
            return IsOnTopEdge(block)
                ? Block4x4(g_BlockB[block], sliceIdB, mbB, mvB, mvdB)
                : Block4x4(g_BlockB[block], sliceIdN, &mbN, mvN, mvdN);
        }

        Block4x4 GetAboveRight(mfxU32 block, mfxU32 blockW) const
        {
            assert(block < 16);
            assert(blockW == 1 || blockW == 2 || blockW == 4);

            mfxU32 blkC = blockW == 1 ? block : blockW == 2 ? block + 1 : block + 5;

            Block4x4 blk4x4 = (blkC == 5)
                ? Block4x4(g_BlockC[blkC], 0, mbC, mvC, 0)
                : IsOnTopEdge(blkC)
                    ? Block4x4(g_BlockC[blkC], sliceIdB, mbB, mvB, 0)
                    : IsOnRightEdge(blkC) || blkC == 3 || blkC == 11
                        ? Block4x4(0, 0, 0, 0, 0)
                        : Block4x4(g_BlockC[blkC], sliceIdN, &mbN, mvN, 0);

            if (blk4x4.mb == 0)
            {
                blk4x4 = (block == 0)
                    ? Block4x4(g_BlockD[block], 0, mbD, mvD, 0)
                    : IsOnTopEdge(block)
                        ? Block4x4(g_BlockD[block], sliceIdB, mbB, mvB, 0)
                        : IsOnLeftEdge(block)
                            ? Block4x4(g_BlockD[block], sliceIdA, mbA, mvA, 0)
                            : Block4x4(g_BlockD[block], sliceIdN, &mbN, mvN, 0);
            }

            return blk4x4;
        }

        MbPartitionIterator parts;
        mfxMbCodeAVC& mbN;
        mfxMbCodeAVC* mbA;
        mfxMbCodeAVC* mbB;
        mfxMbCodeAVC* mbC;
        mfxMbCodeAVC* mbD;
        MbMvs* mvN;
        MbMvs* mvA;
        MbMvs* mvB;
        MbMvs* mvC;
        MbMvs* mvD;
        MbMvs* mvdN;
        MbMvs* mvdA;
        MbMvs* mvdB;
        mfxU16 sliceIdN;
        mfxU16 sliceIdA;
        mfxU16 sliceIdB;

        MbDescriptor& operator=(const MbDescriptor&);
    };

    inline bool IsLeftAvail(const MbDescriptor& mb)
    {
        return mb.mbA != 0;
    }

    inline bool IsAboveAvail(const MbDescriptor& mb)
    {
        return mb.mbB != 0;
    }

    inline bool IsAboveRightAvail(const MbDescriptor& mb)
    {
        return mb.mbC != 0;
    }

    inline bool IsAboveLeftAvail(const MbDescriptor& mb)
    {
        return mb.mbD != 0;
    }

    mfxI16Pair GetMvPred(
        const MbDescriptor& neighbours,
        mfxU32 list,
        mfxU32 block,
        mfxU32 blockW,
        mfxU32 blockH);

    struct PakResources;

    class MbPool
    {
        MbPool(const MbPool&);
        MbPool& operator=(const MbPool&);

    public:
        enum { MB_FREE = 0, MB_LOCKED, MB_DONE };

        MbPool(mfxFrameCUC& cuc, PakResources& res);

        void Reset()
        {
            memset((void *)m_mbDone, 0, m_cuc.FrameParam->AVC.NumMb * sizeof(*m_mbDone));
        }

        bool TryLockMb(mfxU32 mbCnt)
        {
            return vm_interlocked_cas32(&m_mbDone[mbCnt], MB_LOCKED, MB_FREE) == MB_FREE;
        }

        bool IsMbFree(mfxU32 mbCnt)
        {
            return m_mbDone[mbCnt] == MB_FREE;
        }

        bool IsMbReady(mfxU32 mbCnt)
        {
            return m_mbDone[mbCnt] == MB_DONE;
        }

        void MarkMbReady(mfxU32 mbCnt)
        {
            vm_interlocked_xchg32(&m_mbDone[mbCnt], MB_DONE);
        }

        mfxFrameCUC& m_cuc;
        CoeffData* m_coeffs;
        volatile Ipp32u* m_mbDone;
    };

    struct SeqScalingMatrices8x8;

    class ThreadRoutine
    {
    public:
        virtual ~ThreadRoutine(){}
        virtual void Do() = 0;
    };

    class ThreadPredWaveFront : public ThreadRoutine
    {
        ThreadPredWaveFront(const ThreadPredWaveFront&);
        ThreadPredWaveFront& operator=(const ThreadPredWaveFront&);
    public:
        ThreadPredWaveFront(
            MbPool& pool,
            SeqScalingMatrices8x8& scaling8x8,
            bool doDeblocking);
        virtual ~ThreadPredWaveFront(){}

        virtual void Do();

    protected:
        MbPool& m_pool;
        SeqScalingMatrices8x8& m_scaling8x8;
        bool m_doDeblocking;
    };

    class ThreadDeblocking : public ThreadRoutine
    {
        ThreadDeblocking(const ThreadDeblocking&);
        ThreadDeblocking& operator=(const ThreadDeblocking&);
    public:
        explicit ThreadDeblocking(MbPool& pool);
        virtual ~ThreadDeblocking(){}

        virtual void Do();

    protected:
        MbPool& m_pool;
    };

    class PredThread
    {
    public:
        PredThread();
        ~PredThread();

        bool IsValid();

        void Run(ThreadRoutine& routine);

        void Wait();

    private:
        static mfxU32 VM_THREAD_CALLCONVENTION Callback(void* p);

        ThreadRoutine* m_routine;

        UMC::Thread m_thread;
        UMC::Event m_onRun;
        UMC::Event m_onReady;
        bool m_onStop;
    };

    class PredThreadPool
    {
    public:
        PredThreadPool();
        mfxStatus Create(mfxU32 numThread);
        void Destroy();
        void Run(ThreadRoutine& routine);
        void Wait();

    private:
        PredThread* m_threads;
        mfxU32 m_numThread;
    };
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_UTILS_H_
