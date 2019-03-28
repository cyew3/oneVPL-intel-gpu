// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_DEC_STRUCTURES_H__
#define __UMC_H264_DEC_STRUCTURES_H__

#if defined (__ICL)
    // remark #981: operands are evaluated in unspecified order
#pragma warning(disable: 981)
#endif

#include <vector>
#include "umc_memory_allocator.h"
#include "umc_structures.h"
#include "umc_h264_dec_tables.h"

using namespace UMC_H264_DECODER;

//#define STORE_CABAC_BITS

namespace UMC
{
#define ABSOWN(x) ((x) > 0 ? (x) : (-(x)))

typedef int16_t CoeffsCommon;
typedef CoeffsCommon * CoeffsPtrCommon;

// Macroblock type definitions
// Keep these ordered such that intra types are first, followed by
// inter types.  Otherwise you'll need to change the definitions
// of IS_INTRA_MBTYPE and IS_INTER_MBTYPE.
//
// WARNING:  Because the decoder exposes macroblock types to the application,
// these values cannot be changed without affecting users of the decoder.
// If new macroblock types need to be inserted in the middle of the list,
// then perhaps existing types should retain their numeric value, the new
// type should be given a new value, and for coding efficiency we should
// perhaps decouple these values from the ones that are encoded in the
// bitstream.
//

typedef enum {
    MBTYPE_INTRA,            // 4x4
    MBTYPE_INTRA_16x16,
    MBTYPE_INTRA_BL,
    MBTYPE_PCM,              // Raw Pixel Coding, qualifies as a INTRA type...
    MBTYPE_INTER,            // 16x16
    MBTYPE_INTER_16x8,
    MBTYPE_INTER_8x16,
    MBTYPE_INTER_8x8,
    MBTYPE_INTER_8x8_REF0,
    MBTYPE_FORWARD,
    MBTYPE_BACKWARD,
    MBTYPE_SKIPPED,
    MBTYPE_DIRECT,
    MBTYPE_BIDIR,
    MBTYPE_FWD_FWD_16x8,
    MBTYPE_FWD_FWD_8x16,
    MBTYPE_BWD_BWD_16x8,
    MBTYPE_BWD_BWD_8x16,
    MBTYPE_FWD_BWD_16x8,
    MBTYPE_FWD_BWD_8x16,
    MBTYPE_BWD_FWD_16x8,
    MBTYPE_BWD_FWD_8x16,
    MBTYPE_BIDIR_FWD_16x8,
    MBTYPE_BIDIR_FWD_8x16,
    MBTYPE_BIDIR_BWD_16x8,
    MBTYPE_BIDIR_BWD_8x16,
    MBTYPE_FWD_BIDIR_16x8,
    MBTYPE_FWD_BIDIR_8x16,
    MBTYPE_BWD_BIDIR_16x8,
    MBTYPE_BWD_BIDIR_8x16,
    MBTYPE_BIDIR_BIDIR_16x8,
    MBTYPE_BIDIR_BIDIR_8x16,
    MBTYPE_B_8x8,
    NUMBER_OF_MBTYPES
} MB_Type;

// 8x8 Macroblock subblock type definitions
typedef enum {
    SBTYPE_DIRECT = 0,            // B Slice modes
    SBTYPE_8x8 = 1,               // P slice modes
    SBTYPE_8x4 = 2,
    SBTYPE_4x8 = 3,
    SBTYPE_4x4 = 4,
    SBTYPE_FORWARD_8x8 = 5,       // Subtract 4 for mode #
    SBTYPE_BACKWARD_8x8 = 6,
    SBTYPE_BIDIR_8x8 = 7,
    SBTYPE_FORWARD_8x4 = 8,
    SBTYPE_FORWARD_4x8 = 9,
    SBTYPE_BACKWARD_8x4 = 10,
    SBTYPE_BACKWARD_4x8 = 11,
    SBTYPE_BIDIR_8x4 = 12,
    SBTYPE_BIDIR_4x8 = 13,
    SBTYPE_FORWARD_4x4 = 14,
    SBTYPE_BACKWARD_4x4 = 15,
    SBTYPE_BIDIR_4x4 = 16
} SB_Type;

// macro - yields TRUE if a given MB type is INTRA
#define IS_INTRA_MBTYPE_NOT_BL(mbtype) (((mbtype) < MBTYPE_INTER) && ((mbtype) != MBTYPE_INTRA_BL))
#define IS_INTRA_MBTYPE(mbtype) ((mbtype) < MBTYPE_INTER)

// macro - yields TRUE if a given MB type is INTER
#define IS_INTER_MBTYPE(mbtype) ((mbtype) >= MBTYPE_INTER)


typedef uint32_t IntraType;

// This file defines some data structures and constants used by the decoder,
// that are also needed by other classes, such as post filters and
// error concealment.

#define INTERP_FACTOR 4
#define INTERP_SHIFT 2

#define CHROMA_INTERP_FACTOR 8
#define CHROMA_INTERP_SHIFT 3

// at picture edge, clip motion vectors to only this far beyond the edge,
// in pixel units.
#define D_MV_CLIP_LIMIT 19

enum Direction_t{
    D_DIR_FWD = 0,
    D_DIR_BWD = 1,
    D_DIR_BIDIR = 2,
    D_DIR_DIRECT = 3,
    D_DIR_DIRECT_SPATIAL_FWD = 4,
    D_DIR_DIRECT_SPATIAL_BWD = 5,
    D_DIR_DIRECT_SPATIAL_BIDIR = 6
};

inline bool IsForwardOnly(int32_t direction)
{
    return (direction == D_DIR_FWD) || (direction == D_DIR_DIRECT_SPATIAL_FWD);
}

inline bool IsHaveForward(int32_t direction)
{
    return (direction == D_DIR_FWD) || (direction == D_DIR_BIDIR) ||
        (direction == D_DIR_DIRECT_SPATIAL_FWD) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
         (direction == D_DIR_DIRECT);
}

inline bool IsBackwardOnly(int32_t direction)
{
    return (direction == D_DIR_BWD) || (direction == D_DIR_DIRECT_SPATIAL_BWD);
}

inline bool IsHaveBackward(int32_t direction)
{
    return (direction == D_DIR_BWD) || (direction == D_DIR_BIDIR) ||
        (direction == D_DIR_DIRECT_SPATIAL_BWD) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
        (direction == D_DIR_DIRECT);
}

inline bool IsBidirOnly(int32_t direction)
{
    return (direction == D_DIR_BIDIR) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
        (direction == D_DIR_DIRECT);
}

// Warning: If these bit defines change, also need to change same
// defines  and related code in sresidual.s.
enum CBP
{
    D_CBP_LUMA_DC = 0x00001,
    D_CBP_LUMA_AC = 0x1fffe,

    D_CBP_CHROMA_DC = 0x00001,
    D_CBP_CHROMA_AC = 0x1fffe,
    D_CBP_CHROMA_AC_420 = 0x0001e,
    D_CBP_CHROMA_AC_422 = 0x001fe,
    D_CBP_CHROMA_AC_444 = 0x1fffe,

    D_CBP_1ST_LUMA_AC_BITPOS = 1,
    D_CBP_1ST_CHROMA_DC_BITPOS = 17,
    D_CBP_1ST_CHROMA_AC_BITPOS = 19
};

enum
{
    FIRST_DC_LUMA = 0,
    FIRST_AC_LUMA = 1,
    FIRST_DC_CHROMA = 17,
    FIRST_AC_CHROMA = 19
};

inline
uint32_t CreateIPPCBPMask420(uint32_t cbpU, uint32_t cbpV)
{
    uint32_t cbp4x4 = (((cbpU & D_CBP_CHROMA_DC) | ((cbpV & D_CBP_CHROMA_DC) << 1)) << D_CBP_1ST_CHROMA_DC_BITPOS) |
                     ((cbpU & D_CBP_CHROMA_AC_420) << (D_CBP_1ST_CHROMA_AC_BITPOS - 1)) |
                     ((cbpV & D_CBP_CHROMA_AC_420) << (D_CBP_1ST_CHROMA_AC_BITPOS + 4 - 1));
    return cbp4x4;

} // uint32_t CreateIPPCBPMask420(uint32_t nUCBP, uint32_t nVCBP)

inline
unsigned long long CreateIPPCBPMask422(uint32_t cbpU, uint32_t cbpV)
{
    unsigned long long cbp4x4 = (((cbpU & D_CBP_CHROMA_DC) | ((cbpV & D_CBP_CHROMA_DC) << 1)) << D_CBP_1ST_CHROMA_DC_BITPOS) |
                    (((unsigned long long)cbpU & D_CBP_CHROMA_AC_422) << (D_CBP_1ST_CHROMA_AC_BITPOS - 1)) |
                    (((unsigned long long)cbpV & D_CBP_CHROMA_AC_422) << (D_CBP_1ST_CHROMA_AC_BITPOS + 8 - 1));

    return cbp4x4;

} // uint32_t CreateIPPCBPMask422(uint32_t nUCBP, uint32_t nVCBP)

inline
unsigned long long CreateIPPCBPMask444(uint32_t cbpU, uint32_t cbpV)
{
    unsigned long long cbp4x4 = (((cbpU & D_CBP_CHROMA_DC) | ((cbpV & D_CBP_CHROMA_DC) << 1)) << D_CBP_1ST_CHROMA_DC_BITPOS) |
                    (((unsigned long long)cbpU & D_CBP_CHROMA_AC_444) << (D_CBP_1ST_CHROMA_AC_BITPOS - 1)) |
                    (((unsigned long long)cbpV & D_CBP_CHROMA_AC_444) << (D_CBP_1ST_CHROMA_AC_BITPOS + 16 - 1));
    return cbp4x4;

} // uint32_t CreateIPPCBPMask444(uint32_t nUCBP, uint32_t nVCBP)


#define BLOCK_IS_ON_LEFT_EDGE(x) (!((x)&3))
#define BLOCK_IS_ON_TOP_EDGE(x) ((x)<4)

#define CHROMA_BLOCK_IS_ON_LEFT_EDGE(x,c) (x_pos_value[c][x]==0)
#define CHROMA_BLOCK_IS_ON_TOP_EDGE(y,c) (y_pos_value[c][y]==0)

#define GetMBFieldDecodingFlag(x) ((x).mbflags.fdf)
#define GetMBDirectSkipFlag(x) ((x).mbflags.isDirect || (x).mbflags.isSkipped)
#define GetMB8x8TSFlag(x) ((x).mbflags.transform8x8)

#define pGetMBFieldDecodingFlag(x) ((x)->mbflags.fdf)
#define pGetMBDirectSkipFlag(x) ((x)->mbflags.isDirect || (x)->mbflags.isSkipped)
#define pGetMB8x8TSFlag(x) ((x)->mbflags.transform8x8)

#define GetMBSkippedFlag(x) ((x).mbflags.isSkipped)
#define pGetMBSkippedFlag(x) ((x)->mbflags.isSkipped)

#define pSetMBDirectFlag(x)  ((x)->mbflags.isDirect = 1);
#define SetMBDirectFlag(x)  ((x).mbflags.isDirect = 1);

#define pSetMBSkippedFlag(x)  ((x)->mbflags.isSkipped = 1);
#define SetMBSkippedFlag(x)  ((x).mbflags.isSkipped = 1);

#define pSetMBFieldDecodingFlag(x,y)     \
    (x->mbflags.fdf = (uint8_t)y)

#define SetMBFieldDecodingFlag(x,y)     \
    (x.mbflags.fdf = (uint8_t)y)

#define pSetMB8x8TSFlag(x,y)            \
    (x->mbflags.transform8x8 = (uint8_t)y)

#define SetMB8x8TSFlag(x,y)             \
    (x.mbflags.transform8x8 = (uint8_t)y)

#define pSetPairMBFieldDecodingFlag(x1,x2,y)    \
    (x1->mbflags.fdf = (uint8_t)y);    \
    (x2->mbflags.fdf = (uint8_t)y)

#define SetPairMBFieldDecodingFlag(x1,x2,y)     \
    (x1.mbflags.fdf = (uint8_t)y);    \
    (x2.mbflags.fdf = (uint8_t)y)

///////////////// New structures

#pragma pack(push, 1)

struct H264DecoderMotionVector
{
    int16_t mvx;
    int16_t mvy;
}; // 4 bytes

typedef int8_t RefIndexType;

struct H264DecoderMacroblockRefIdxs
{
    RefIndexType refIndexs[4];                              // 4 bytes

};//4 bytes

struct H264DecoderMacroblockMVs
{
    H264DecoderMotionVector MotionVectors[16];                  // (H264DecoderMotionVector []) motion vectors for each block in macroblock

}; // 64 bytes

typedef uint8_t NumCoeffsType;
struct H264DecoderMacroblockCoeffsInfo
{
    NumCoeffsType numCoeffs[48];                                         // (uint8_t) number of coefficients in each block in macroblock

}; // 24 bytes for YUV420. For YUV422, YUV444 support need to extend it

struct H264MBFlags
{
    uint8_t fdf : 1;
    uint8_t transform8x8 : 1;
    uint8_t isDirect : 1;
    uint8_t isSkipped : 1;
};

struct H264DecoderMacroblockGlobalInfo
{
    int8_t sbtype[4];                                            // (uint8_t []) types of subblocks in macroblock
    int16_t slice_id;                                            // (int16_t) number of slice
    int8_t mbtype;                                               // (uint8_t) type of macroblock
    H264MBFlags mbflags;

    H264DecoderMacroblockRefIdxs refIdxs[2];
}; // 16 bytes

struct H264DecoderMacroblockLocalInfo
{
    uint32_t cbp4x4_luma;                                         // (uint32_t) coded block pattern of luma blocks
    uint32_t cbp4x4_chroma[2];                                    // (uint32_t []) coded block patterns of chroma blocks
    uint8_t cbp;
    int8_t QP;

    union
    {
        int8_t sbdir[4];
        struct
        {
            uint16_t edge_type;
            uint8_t intra_chroma_mode;
        } IntraTypes;
    };
}; // 20 bytes

struct H264DecoderBlockLocation
{
    int32_t mb_num;                                              // (int32_t) number of owning macroblock
    int32_t block_num;                                           // (int32_t) number of block

}; // 8 bytes

struct H264DecoderMacroblockNeighboursInfo
{
    int32_t mb_A;                                                // (int32_t) number of left macroblock
    int32_t mb_B;                                                // (int32_t) number of top macroblock
    int32_t mb_C;                                                // (int32_t) number of right-top macroblock
    int32_t mb_D;                                                // (int32_t) number of left-top macroblock

}; // 32 bytes

struct H264DecoderBlockNeighboursInfo
{
    H264DecoderBlockLocation mbs_left[4];
    H264DecoderBlockLocation mb_above;
    H264DecoderBlockLocation mb_above_right;
    H264DecoderBlockLocation mb_above_left;
    H264DecoderBlockLocation mbs_left_chroma[2][4];
    H264DecoderBlockLocation mb_above_chroma[2];
    int32_t m_bInited;

}; // 128 bytes

struct H264DecoderMacroblockLayerInfo
{
    int8_t sbtype[4];
    int8_t sbdir[4];
    int8_t mbtype;
    H264MBFlags mbflags;
    H264DecoderMacroblockRefIdxs refIdxs[2];
};

#pragma pack(pop)

//this structure is present in each decoder frame
struct H264DecoderGlobalMacroblocksDescriptor
{
    H264DecoderMacroblockMVs *MV[2];//MotionVectors L0 L1
    H264DecoderMacroblockGlobalInfo *mbs;//macroblocks
};

struct H264DecoderBaseFrameDescriptor
{
    int32_t m_PictureStructureForDec;
    int32_t m_PictureStructureForRef;
    int32_t totalMBs;
    int32_t m_PicOrderCnt[2];
    int32_t m_bottom_field_flag[2];
    bool m_isShortTermRef[2];
    bool m_isLongTermRef[2];
    bool m_isInterViewRef;

    H264DecoderGlobalMacroblocksDescriptor m_mbinfo;
};

//this structure is one(or couple) for all decoder
class H264DecoderFrame;
class H264Slice;

class H264DecoderLocalMacroblockDescriptor
{
public:
    // Default constructor
    H264DecoderLocalMacroblockDescriptor(void);
    // Destructor
    ~H264DecoderLocalMacroblockDescriptor(void);

    // Allocate decoding data
    bool Allocate(int32_t iMBCount, MemoryAllocator *pMemoryAllocator);

    H264DecoderMacroblockMVs *(MVDeltas[2]);                    // (H264DecoderMacroblockMVs * ([])) motionVectors Deltas L0 and L1
    H264DecoderMacroblockCoeffsInfo *MacroblockCoeffsInfo;      // (H264DecoderMacroblockCoeffsInfo *) info about num_coeffs in each block in the current  picture
    H264DecoderMacroblockLocalInfo *mbs;                        // (H264DecoderMacroblockLocalInfo *) reconstuction info
    H264DecoderMBAddr *active_next_mb_table;                    // (H264DecoderMBAddr *) current "next addres" table

    // Assignment operator
    H264DecoderLocalMacroblockDescriptor &operator = (H264DecoderLocalMacroblockDescriptor &);

    bool m_isBusy;
    H264DecoderFrame * m_pFrame;

protected:
    // Release object
    void Release(void);

    uint8_t *m_pAllocated;                                        // (uint8_t *) pointer to allocated memory
    MemID m_midAllocated;                                       // (MemID) mem id of allocated memory
    size_t m_nAllocatedSize;                                    // (size_t) size of allocated memory

    MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory management tool
};

#define INLINE inline
// __forceinline

INLINE H264DecoderMacroblockMVs * GetMVs(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum) {return &gmbinfo->MV[list][mbNum];}

INLINE H264DecoderMotionVector * GetMVDelta(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum) {return gmbinfo->MV[2 + list][mbNum].MotionVectors;}

INLINE H264DecoderMotionVector & GetMV(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum, int32_t blockNum) {return gmbinfo->MV[list][mbNum].MotionVectors[blockNum];}


INLINE NumCoeffsType * GetNumCoeffs(H264DecoderLocalMacroblockDescriptor *lmbinfo, int32_t mbNum) {return lmbinfo->MacroblockCoeffsInfo[mbNum].numCoeffs;}

INLINE const NumCoeffsType & GetNumCoeff(H264DecoderLocalMacroblockDescriptor *lmbinfo, int32_t mbNum, int32_t block) {return lmbinfo->MacroblockCoeffsInfo[mbNum].numCoeffs[block];}


INLINE RefIndexType * GetRefIdxs(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum) {return gmbinfo->mbs[mbNum].refIdxs[list].refIndexs;}

INLINE const RefIndexType & GetRefIdx(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum, int32_t block) {return gmbinfo->mbs[mbNum].refIdxs[list].refIndexs[block];}

INLINE const RefIndexType * GetReferenceIndexPtr(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum, int32_t block)
{
    return &gmbinfo->mbs[mbNum].refIdxs[list].refIndexs[subblock_block_membership[block]];
}

INLINE const RefIndexType & GetReferenceIndex(H264DecoderGlobalMacroblocksDescriptor *gmbinfo, int32_t list, int32_t mbNum, int32_t block)
{
    return *GetReferenceIndexPtr(gmbinfo, list, mbNum, block);
}


INLINE RefIndexType GetReferenceIndex(RefIndexType *refIndxs, int32_t block)
{
    return refIndxs[subblock_block_membership[block]];
}
/*
inline RefIndexType* GetReferenceIndexPtr(RefIndexType *refIndxs, int32_t block)
{
    return &(refIndxs[subblock_block_membership[block]]);
}*/

class Macroblock
{
    Macroblock(uint32_t mbNum, H264DecoderGlobalMacroblocksDescriptor *gmbinfo, H264DecoderLocalMacroblockDescriptor *lmbinfo)
    {
        GlobalMacroblockInfo = &gmbinfo->mbs[mbNum];
        LocalMacroblockInfo = &lmbinfo->mbs[mbNum];
        MacroblockCoeffsInfo = &lmbinfo->MacroblockCoeffsInfo[mbNum];
    }

    inline H264DecoderMacroblockMVs * GetMV(int32_t list) {return MVs[list];}

    inline H264DecoderMacroblockMVs * GetMVDelta(int32_t list) {return MVs[2 + list];}

    inline H264DecoderMacroblockRefIdxs * GetRefIdx(int32_t list) {return RefIdxs[list];}

    inline H264DecoderMacroblockGlobalInfo * GetGlobalInfo() {return GlobalMacroblockInfo;}

    inline H264DecoderMacroblockLocalInfo * GetLocalInfo() {return LocalMacroblockInfo;}

private:

    H264DecoderMacroblockMVs *MVs[4];//MV L0,L1, MVDeltas 0,1
    H264DecoderMacroblockRefIdxs *RefIdxs[2];//RefIdx L0, L1
    H264DecoderMacroblockCoeffsInfo *MacroblockCoeffsInfo;
    H264DecoderMacroblockGlobalInfo *GlobalMacroblockInfo;
    H264DecoderMacroblockLocalInfo *LocalMacroblockInfo;
};

struct H264DecoderCurrentMacroblockDescriptor
{
    H264DecoderMacroblockMVs *MVs[2];//MV L0,L1,
    H264DecoderMacroblockMVs *MVDelta[2];//MVDeltas L0,L1
    H264DecoderMacroblockNeighboursInfo CurrentMacroblockNeighbours;//mb neighboring info
    H264DecoderBlockNeighboursInfo CurrentBlockNeighbours;//block neighboring info (if mbaff turned off remained static)
    H264DecoderMacroblockGlobalInfo *GlobalMacroblockInfo;
    H264DecoderMacroblockGlobalInfo *GlobalMacroblockPairInfo;
    H264DecoderMacroblockLocalInfo *LocalMacroblockInfo;
    H264DecoderMacroblockLocalInfo *LocalMacroblockPairInfo;

    H264DecoderMotionVector & GetMV(int32_t list, int32_t blockNum) {return MVs[list]->MotionVectors[blockNum];}

    H264DecoderMotionVector * GetMVPtr(int32_t list, int32_t blockNum) {return &MVs[list]->MotionVectors[blockNum];}

    INLINE const RefIndexType & GetRefIdx(int32_t list, int32_t block) const {return RefIdxs[list]->refIndexs[block];}

    INLINE const RefIndexType & GetReferenceIndex(int32_t list, int32_t block) const {return RefIdxs[list]->refIndexs[subblock_block_membership[block]];}

    INLINE H264DecoderMacroblockRefIdxs* GetReferenceIndexStruct(int32_t list)
    {
        return RefIdxs[list];
    }

    INLINE H264DecoderMacroblockCoeffsInfo * GetNumCoeffs() {return MacroblockCoeffsInfo;}
    INLINE const NumCoeffsType & GetNumCoeff(int32_t block) {return MacroblockCoeffsInfo->numCoeffs[block];}

    H264DecoderMacroblockRefIdxs *RefIdxs[2];//RefIdx L0, L1
    H264DecoderMacroblockCoeffsInfo *MacroblockCoeffsInfo;

    bool isInited;
};

template<class T>
inline void swapValues(T & t1, T & t2)
{
    T temp = t1;
    t1 = t2;
    t2 = temp;
}

template<typename T>
inline void storeInformationInto8x8(T* info, T value)
{
    info[0] = value;
    info[1] = value;
    info[4] = value;
    info[5] = value;
}

template<typename T>
inline void storeStructInformationInto8x8(T* info, const T &value)
{
    info[0] = value;
    info[1] = value;
    info[4] = value;
    info[5] = value;
}

template<typename T>
inline void fill_n(T *first, size_t count, T val)
{   // copy _Val _Count times through [_First, ...)
    for (; 0 < count; --count, ++first)
        *first = val;
}

template<typename T>
inline void fill_struct_n(T *first, size_t count, const T& val)
{   // copy _Val _Count times through [_First, ...)
    for (; 0 < count; --count, ++first)
        *first = val;
}

inline int32_t GetReferenceField(ReferenceFlags *pFields, int32_t RefIndex)
{
    VM_ASSERT(pFields[RefIndex].field >= 0);
    return pFields[RefIndex].field;
}

struct H264IntraTypesProp
{
    H264IntraTypesProp()
    {
        Reset();
    }

    int32_t m_nSize;                                             // (int32_t) size of allocated intra type array
    MemID m_mid;                                                // (MemID) mem id of allocated buffer for intra types

    void Reset(void)
    {
        m_nSize = 0;
        m_mid = MID_INVALID;
    }
};

extern const UMC::H264DecoderMotionVector zeroVector;

} // end namespace UMC

#endif // __UMC_H264_DEC_STRUCTURES_H__
#endif // MFX_ENABLE_H264_VIDEO_DECODE
