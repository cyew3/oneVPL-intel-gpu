#ifndef __H264_CABAC_H
#define __H264_CABAC_H

#include "bs_reader.h"
#include "h264_struct.h"
#include <vector>
#include <list>
#include <bitset>

class H264_BitStream;

namespace BS_H264{

typedef enum SEext{
      SE_MB_SKIP_FLAG = 0
    , SE_END_OF_SLICE_FLAG
    , SE_MB_FIELD_DECODING_FLAG
    , SE_MB_TYPE
    , SE_TRANSFORM_SIZE_8X8_FLAG
    , SE_CODED_BLOCK_PATTERN
    , SE_PREV_INTRA_PRED_MODE_FLAG
    , SE_REM_INTRA_PRED_MODE
    , SE_INTRA_CHROMA_PRED_MODE
    , SE_CODED_BLOCK_FLAG
    , SE_SIGNIFICANT_COEFF_FLAG
    , SE_LAST_SIGNIFICANT_COEFF_FLAG
    , SE_COEFF_ABS_LEVEL_MINUS1
    , SE_COEFF_SIGN_FLAG
    , SE_SUB_MB_TYPE
    , SE_REF_IDX_L0
    , SE_REF_IDX_L1
    , SE_MVD_L0
    , SE_MVD_L1
}SEext;

enum SE{
      MB_TYPE_SI  = 0
    , MB_TYPE_I
    , MB_SKIP_FLAG_P_SP
    , MB_TYPE_P_SP
    , SUB_MB_TYPE_P_SP
    , MB_SKIP_FLAG_B
    , MB_TYPE_B
    , SUB_MB_TYPE_B
    , MVD_LX_0
    , MVD_LX_1
    , REF_IDX_LX
    , MB_QP_DELTA
    , INTRA_CHROMA_PRED_MODE
    , PREV_INTRA4X4_PRED_MODE_FLAG
    , PREV_INTRA8X8_PRED_MODE_FLAG = PREV_INTRA4X4_PRED_MODE_FLAG
    , REM_INTRA4X4_PRED_MODE
    , REM_INTRA8X8_PRED_MODE = REM_INTRA4X4_PRED_MODE
    , MB_FIELD_DECODING_FLAG
    , CODED_BLOCK_PATTERN_LUMA
    , CODED_BLOCK_PATTERN_CHROMA = CODED_BLOCK_PATTERN_LUMA
    , num_SE_main
    , CODED_BLOCK_FLAG_LT5 = num_SE_main
    , SIGNIFICANT_COEFF_FLAG_FRAME_LT5
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_LT5
    , COEFF_ABS_LEVEL_MINUS1_LT5
    , COEFF_SIGN_FLAG
    , END_OF_SLICE_FLAG
    , SIGNIFICANT_COEFF_FLAG_FIELD_LT5
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_LT5
    , TRANSFORM_SIZE_8X8_FLAG
    , SIGNIFICANT_COEFF_FLAG_FRAME_EQ5
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_EQ5
    , COEFF_ABS_LEVEL_MINUS1_EQ5
    , SIGNIFICANT_COEFF_FLAG_FIELD_EQ5
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_EQ5
    , CODED_BLOCK_FLAG_GT5_LT9
    , CODED_BLOCK_FLAG_GT9_LT13
    , CODED_BLOCK_FLAG_EQ_5_9_13
    , CODED_BLOCK_FLAG_EQ5  = CODED_BLOCK_FLAG_EQ_5_9_13
    , CODED_BLOCK_FLAG_EQ9  = CODED_BLOCK_FLAG_EQ_5_9_13
    , CODED_BLOCK_FLAG_EQ13 = CODED_BLOCK_FLAG_EQ_5_9_13
    , SIGNIFICANT_COEFF_FLAG_FRAME_GT5_LT9
    , SIGNIFICANT_COEFF_FLAG_FRAME_GT9_LT13
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_GT5_LT9
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_GT9_LT13
    , COEFF_ABS_LEVEL_MINUS1_GT5_LT9
    , COEFF_ABS_LEVEL_MINUS1_GT9_LT13
    , SIGNIFICANT_COEFF_FLAG_FIELD_GT5_LT9
    , SIGNIFICANT_COEFF_FLAG_FIELD_GT9_LT13
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_GT5_LT9
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_GT9_LT13
    , SIGNIFICANT_COEFF_FLAG_FRAME_EQ9
    , SIGNIFICANT_COEFF_FLAG_FRAME_EQ13
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_EQ9
    , LAST_SIGNIFICANT_COEFF_FLAG_FRAME_EQ13
    , COEFF_ABS_LEVEL_MINUS1_EQ9
    , COEFF_ABS_LEVEL_MINUS1_EQ13
    , SIGNIFICANT_COEFF_FLAG_FIELD_EQ9
    , SIGNIFICANT_COEFF_FLAG_FIELD_EQ13
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_EQ9
    , LAST_SIGNIFICANT_COEFF_FLAG_FIELD_EQ13
    , num_SE
};

enum CTX_FLAGS{
      ERROR = -4
    , EXTERNAL
    , TERMINATE
    , BYPASS
};

enum MB_PART_PRED_MODE{
      Intra_4x4
    , Intra_8x8
    , Intra_16x16
    , Pred_L0
    , Pred_L1
    , BiPred
    , Direct
    , MODE_NA = 127
};

enum MB_TYPE_NAME{
      MBTN_I_start = 0
    , I_NxN = MBTN_I_start
    , I_16x16_0_0_0
    , I_16x16_1_0_0
    , I_16x16_2_0_0
    , I_16x16_3_0_0
    , I_16x16_0_1_0
    , I_16x16_1_1_0
    , I_16x16_2_1_0
    , I_16x16_3_1_0
    , I_16x16_0_2_0
    , I_16x16_1_2_0
    , I_16x16_2_2_0
    , I_16x16_3_2_0
    , I_16x16_0_0_1
    , I_16x16_1_0_1
    , I_16x16_2_0_1
    , I_16x16_3_0_1
    , I_16x16_0_1_1
    , I_16x16_1_1_1
    , I_16x16_2_1_1
    , I_16x16_3_1_1
    , I_16x16_0_2_1
    , I_16x16_1_2_1
    , I_16x16_2_2_1
    , I_16x16_3_2_1
    , I_PCM
    , MBTN_SI
    , MBTN_P_start
    , P_L0_16x16 = MBTN_P_start
    , P_L0_L0_16x8
    , P_L0_L0_8x16
    , P_8x8
    , P_8x8ref0
    , MBTN_B_start
    , B_Direct_16x16 = MBTN_B_start
    , B_L0_16x16
    , B_L1_16x16
    , B_Bi_16x16
    , B_L0_L0_16x8
    , B_L0_L0_8x16
    , B_L1_L1_16x8
    , B_L1_L1_8x16
    , B_L0_L1_16x8
    , B_L0_L1_8x16
    , B_L1_L0_16x8
    , B_L1_L0_8x16
    , B_L0_Bi_16x8
    , B_L0_Bi_8x16
    , B_L1_Bi_16x8
    , B_L1_Bi_8x16
    , B_Bi_L0_16x8
    , B_Bi_L0_8x16
    , B_Bi_L1_16x8
    , B_Bi_L1_8x16
    , B_Bi_Bi_16x8
    , B_Bi_Bi_8x16
    , B_8x8
    , P_Skip
    , B_Skip
    , MBTN_SubP_start
    , P_L0_8x8 = MBTN_SubP_start
    , P_L0_8x4
    , P_L0_4x8
    , P_L0_4x4
    , MBTN_SubB_start
    , B_Direct_8x8 = MBTN_SubB_start
    , B_L0_8x8
    , B_L1_8x8
    , B_Bi_8x8
    , B_L0_8x4
    , B_L0_4x8
    , B_L1_8x4
    , B_L1_4x8
    , B_Bi_8x4
    , B_Bi_4x8
    , B_L0_4x4
    , B_L1_4x4
    , B_Bi_4x4
};


enum BLK_TYPE{
      BLK_TYPE_Cb     = 0x01
    , BLK_TYPE_Cr     = 0x02
    , BLK_TYPE_Chroma = 0x04
    , BLK_TYPE_DC     = 0x08
    , BLK_TYPE_AC     = 0x10
    , BLK_TYPE_4x4    = 0x20
    , BLK_TYPE_8x8    = 0x40
    , BLK_TYPE_16x16  = 0x80
};


enum {
      Intra16x16DCLevel   = (BLK_TYPE_16x16|BLK_TYPE_DC)
    , Intra16x16ACLevel   = (BLK_TYPE_16x16|BLK_TYPE_AC)
    , LumaLevel4x4        = (BLK_TYPE_4x4)
    , ChromaDCLevel       = (BLK_TYPE_Chroma|BLK_TYPE_DC)
    , ChromaACLevel       = (BLK_TYPE_Chroma|BLK_TYPE_AC)
    , LumaLevel8x8        = (BLK_TYPE_8x8)
    , CbIntra16x16DCLevel = (BLK_TYPE_Cb|BLK_TYPE_16x16|BLK_TYPE_DC)
    , CbIntra16x16ACLevel = (BLK_TYPE_Cb|BLK_TYPE_16x16|BLK_TYPE_AC)
    , CbLumaLevel4x4      = (BLK_TYPE_Cb|BLK_TYPE_4x4)
    , CbLumaLevel8x8      = (BLK_TYPE_Cb|BLK_TYPE_8x8)
    , CrIntra16x16DCLevel = (BLK_TYPE_Cr|BLK_TYPE_16x16|BLK_TYPE_DC)
    , CrIntra16x16ACLevel = (BLK_TYPE_Cr|BLK_TYPE_16x16|BLK_TYPE_AC)
    , CrLumaLevel4x4      = (BLK_TYPE_Cr|BLK_TYPE_4x4)
    , CrLumaLevel8x8      = (BLK_TYPE_Cr|BLK_TYPE_8x8)
};

inline bool isI (slice_header& s) { return (s.slice_type%5 == 2); };
inline bool isP (slice_header& s) { return (s.slice_type%5 == 0); };
inline bool isB (slice_header& s) { return (s.slice_type%5 == 1); };
inline bool isSP(slice_header& s) { return (s.slice_type%5 == 3); };
inline bool isSI(slice_header& s) { return (s.slice_type%5 == 4); };

struct part_loc{
    Bs32s mbAddr;
    Bs8s  mbPartIdx;
    Bs8s  subMbPartIdx;
};

struct SDecCtx{
    void  update(slice_header& s, bool newPicture);
    void  updateCtxBlockCat();
    void  setCBF(bool f);
    void  setCT(Bs16u f);
    Bs16u getCT(macro_block& mb, Bs16u type, Bs16u idx4x4, Bs8u iCbCr);
    Bs32u NextMbAddress(Bs32u n);
    Bs16u mapSE(SEext se);
    Bs8u  defaultMBFDF();
    Bs8s  MbPartPredMode(macro_block& mb, Bs8u x);
    Bs8u  NumMbPart(macro_block& mb);
    Bs8u  MbTypeName(macro_block& mb);
    Bs8u  MbPartWidth(Bs8u MbType);
    Bs8u  MbPartHeight(Bs8u MbType);
    Bs8s  SubMbPredMode(macro_block& mb, Bs8u idx);
    Bs8u  NumSubMbPart(macro_block& mb, Bs8u idx);
    Bs8u  SubMbTypeName(macro_block& mb, Bs8u idx);
    Bs8u  SubMbPartWidth(macro_block& mb, Bs8u idx);
    Bs8u  SubMbPartHeight(macro_block& mb, Bs8u idx);
    Bs8s  CodedBlockPatternLuma(macro_block& mb);
    Bs8s  CodedBlockPatternChroma(macro_block& mb);
    Bs32s mapME(Bs32u codeNum);
    
    mb_location  getNeighbLoc(bool isLuma, Bs16s xN, Bs16s yN); //6.4.12
    std::pair<Bs32s, Bs32s> neighb8x8Y(Bs32s luma8x8BlkIdx,   Bs16s xD, Bs16s yD);
    std::pair<Bs32s, Bs32s> neighb8x8C(Bs32s cx8x8BlkIdx,     Bs16s xD, Bs16s yD);
    std::pair<Bs32s, Bs32s> neighb4x4Y(Bs32s luma4x4BlkIdx,   Bs16s xD, Bs16s yD);
    std::pair<Bs32s, Bs32s> neighb4x4C(Bs32s chroma4x4BlkIdx, Bs16s xD, Bs16s yD);

    Bs16s    predPartWidth(Bs8s mbPartIdx);
    part_loc neighbPart(Bs8s mbPartIdx, Bs8s subMbPartIdx, Bs16s xD, Bs16s yD); 
    
    inline macro_block* getMB(Bs32s addr) { return addr >= 0 ? MbsByAddr[addr] : 0; };

    inline Bs32s neighbMbA(bool isLuma = true) { return getNeighbLoc(isLuma, -1,  0).Addr; }; // 6.4.11.1
    inline Bs32s neighbMbB(bool isLuma = true) { return getNeighbLoc(isLuma,  0, -1).Addr; }; // 6.4.11.1

    inline std::pair<Bs32s, Bs32s> neighb8x8YblkA(Bs32s luma8x8BlkIdx  ) { return neighb8x8Y(luma8x8BlkIdx,   -1,  0); }; // 6.4.11.2
    inline std::pair<Bs32s, Bs32s> neighb8x8YblkB(Bs32s luma8x8BlkIdx  ) { return neighb8x8Y(luma8x8BlkIdx,    0, -1); }; // 6.4.11.2
    inline std::pair<Bs32s, Bs32s> neighb8x8CblkA(Bs32s chroma8x8BlkIdx) { return neighb8x8C(chroma8x8BlkIdx, -1,  0); }; // 6.4.11.3
    inline std::pair<Bs32s, Bs32s> neighb8x8CblkB(Bs32s chroma8x8BlkIdx) { return neighb8x8C(chroma8x8BlkIdx,  0, -1); }; // 6.4.11.3
    inline std::pair<Bs32s, Bs32s> neighb4x4YblkA(Bs32s luma4x4BlkIdx  ) { return neighb4x4Y(luma4x4BlkIdx,   -1,  0); }; // 6.4.11.4
    inline std::pair<Bs32s, Bs32s> neighb4x4YblkB(Bs32s luma4x4BlkIdx  ) { return neighb4x4Y(luma4x4BlkIdx,    0, -1); }; // 6.4.11.4
    inline std::pair<Bs32s, Bs32s> neighb4x4CblkA(Bs32s chroma4x4BlkIdx) { return neighb4x4C(chroma4x4BlkIdx, -1,  0); }; // 6.4.11.5
    inline std::pair<Bs32s, Bs32s> neighb4x4CblkB(Bs32s chroma4x4BlkIdx) { return neighb4x4C(chroma4x4BlkIdx,  0, -1); }; // 6.4.11.5
    inline part_loc neighbPartA(Bs8s mbPartIdx, Bs8s subMbPartIdx){ return neighbPart(mbPartIdx, subMbPartIdx,-1,  0); }; // 6.4.11.7
    inline part_loc neighbPartB(Bs8s mbPartIdx, Bs8s subMbPartIdx){ return neighbPart(mbPartIdx, subMbPartIdx, 0, -1); }; // 6.4.11.7

    // 6.4.8 - 6.4.10
    inline Bs32s mbAddrA   ()            { return (MbaffFrameFlag + 1) * (curMb->Addr / (MbaffFrameFlag + 1) - 1); }
    inline Bs32s mbAddrB   ()            { return (MbaffFrameFlag + 1) * (curMb->Addr / (MbaffFrameFlag + 1) - PicWidthInMbs); }
    inline Bs32s mbAddrC   ()            { return (MbaffFrameFlag + 1) * (curMb->Addr / (MbaffFrameFlag + 1) - PicWidthInMbs + 1); }
    inline Bs32s mbAddrD   ()            { return (MbaffFrameFlag + 1) * (curMb->Addr / (MbaffFrameFlag + 1) - PicWidthInMbs - 1); }
    inline bool  Available (Bs32s mbAddr){ return !(mbAddr < 0 || mbAddr > (Bs32s)curMb->Addr || MbToSliceIdMap[mbAddr] != MbToSliceIdMap[curMb->Addr]); }
    inline bool  AvailableA()            { return ((curMb->Addr / (MbaffFrameFlag + 1)) % PicWidthInMbs == 0) ? false : Available(mbAddrA()); }
    inline bool  AvailableB()            { return Available(mbAddrB()); }
    inline bool  AvailableC()            { return ((curMb->Addr / (MbaffFrameFlag + 1) + 1) % PicWidthInMbs == 0) ? false : Available(mbAddrC()); }
    inline bool  AvailableD()            { return ((curMb->Addr / (MbaffFrameFlag + 1)) % PicWidthInMbs == 0) ? false : Available(mbAddrD()); }

    slice_header* slice;
    macro_block*  mbs;
    macro_block*  curMb;

    Bs16u MbWidthC;
    Bs16u MbHeightC;
    Bs16u BitDepthY;
    Bs16u QpBdOffsetY;
    Bs16u BitDepthC;
    Bs16u QpBdOffsetC;
    Bs16u RawMbBits;
    Bs16u PicWidthInMbs;
    Bs16u PicWidthInSamplesL;
    Bs16u PicWidthInSamplesC;
    Bs16u PicHeightInMapUnits;
    Bs16u PicSizeInMapUnits;
    Bs16u FrameHeightInMbs;
    Bs16u MbaffFrameFlag;
    Bs16u PicHeightInMbs;
    Bs16u PicHeightInSamplesL;
    Bs16u PicHeightInSamplesC;
    Bs16u PicSizeInMbs;
    Bs8u  ChromaArrayType;
    Bs8u  SubWidthC;
    Bs8u  SubHeightC;
    Bs8u  NumC8x8;
    Bs8u  ctxBlockCat;
    Bs8u  blk_type;
    Bs8u  blkIdx;
    Bs8u  iCbCr;
    Bs8s  levelListIdx;
    Bs8u  numDecodAbsLevelEq1;
    Bs8u  numDecodAbsLevelGt1;
    Bs8u  mbPartIdx;
    Bs8u  lX;
    Bs8u  subMbPartIdx;
    Bs8u  compIdx;

    std::vector<Bs32u> mapUnitToSliceGroupMap;
    std::vector<Bs32u> MbToSliceGroupMap;
    std::vector<Bs16s> MbToSliceIdMap;
    std::vector<macro_block*> MbsByAddr;
};

struct Bin{
    static const Bs16u capacity = 32;
    static const Bs32u ff = 0xFFFFFFFF;

    Bin(Bs32u bin_val, Bs8u size) 
        : n(size)
        , b(bin_val & ( ff >> (capacity-size))){ 
    }
    bool operator == (const Bin &bin) const { 
        return (bin.n == n && bin.b.to_ulong() == b.to_ulong()); 
    }
    void put (Bs32u bits, Bs8u num = 1) {
        if(!num) return;
        b <<= num;
        b |= std::bitset<capacity>(bits & ( ff >> (capacity-num)));
        n += num;
    }
    Bs32u get() { 
        return b.to_ulong(); 
    }

    Bs8u  n;
    std::bitset<capacity> b;
};

struct BinPair{
    BinPair(Bin p = Bin(0,0), Bin s = Bin(0,0), Bs32s v = 0)
        : prefix(p)
        , suffix(s)
        , value(v)
    {};
    Bin& operator [] (const Bs16u idx) {
        return idx ? suffix : prefix;
    }

    Bin   prefix;
    Bin   suffix;
    Bs32s value;
};

typedef std::list<BinPair> Bins;

class H264_CABAC{
public:
    H264_CABAC(H264_BitStream& reader);
    ~H264_CABAC(){
    };

    void  InitSlice(){
        init_ctx();
        init_ade();
        m_c = 0;
    };
    void  InitAfterPCM(){ init_ade(); };

    Bs32s ae(Bs16u se);
    Bs32s mbqpd();

    Bs64u nBits;
    Bs64u nBins;

private:
    static const Bs16u CtxTblSize = 1024; 
    static const Bs16s ctxIdxOffset[num_SE][2];
    static const Bs8s  ctxIdxIncTbl[num_SE_main][2][7];

    H264_BitStream& reader;
    BSErr&     last_err;
    SDecCtx&   ctx;
    Bs8u       m_CtxState[CtxTblSize];
    Bs16u      m_codIRange;
    Bs16u      m_codIOffset;
    Bins       m_binFL1;
    Bins       m_binMvdLx;
    Bins       m_binRefIdxLx;
    Bins       m_binTU3;
    Bins       m_binFL7;
    Bins       m_binCoeffAbsLvl;
    Bins       m_binMbTypeSI;
    Bins       m_binMbTypeI;
    Bins       m_binMbTypeP;
    Bins       m_binMbTypeB;
    Bins       m_binSubMbTypeP;
    Bins       m_binSubMbTypeB;
    Bins       m_binCBPtrCatEq03;
    Bins       m_binCBPtrCatNe03;

    Bs32u     m_c;
    Bs32u     m_n;
    Bs32u     m_decBins;
    FILE*     m_log;
    
    void init_ctx();
    void init_ade();
    void init_bin();

    void RenormD         ();
    Bs8u DecodeDecision  (Bs8u& ctxState);
    Bs8u DecodeBypass    ();
    Bs8u DecodeTerminate ();

    Bins& GetBinarization(Bs16u se);
    Bs16s ctxInc(Bs16u se, Bs16u binIdx, bool isSuffix);

    bool  cbfCondTermFlagN(bool isA);
    Bs8s  cbSeCtxIdxInc(Bs16u se, Bs16u binIdx);
    bool  ridxCondTermFlagN(bool isA);
    Bs16u absMvdCompN(bool isA);

};
} // namespace BS_H264

#endif //#ifndef __H264_CABAC_H