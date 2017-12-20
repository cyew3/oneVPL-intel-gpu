#pragma once

#include "bs_reader2.h"
#include "avc2_struct.h"
#include <vector>
#include <algorithm>

namespace BS_AVC2
{

struct BlkLoc
{
    Bs32s idx;
    Bs32s addr;
};

struct PartLoc
{
    Bs32s mbAddr;
    Bs8s  mbPartIdx;
    Bs8s  subMbPartIdx;
};

enum BLK_TYPE
{
      BLK_Cb     = 0x01
    , BLK_Cr     = 0x02
    , BLK_Chroma = 0x04
    , BLK_DC     = 0x08
    , BLK_AC     = 0x10
    , BLK_4x4    = 0x20
    , BLK_8x8    = 0x40
    , BLK_16x16  = 0x80
    , Intra16x16DCLevel   = (BLK_16x16|BLK_DC)
    , Intra16x16ACLevel   = (BLK_16x16|BLK_AC)
    , LumaLevel4x4        = (BLK_4x4)
    , ChromaDCLevel       = (BLK_Chroma|BLK_DC)
    , ChromaACLevel       = (BLK_Chroma|BLK_AC)
    , LumaLevel8x8        = (BLK_8x8)
    , CbIntra16x16DCLevel = (BLK_Cb|BLK_16x16|BLK_DC)
    , CbIntra16x16ACLevel = (BLK_Cb|BLK_16x16|BLK_AC)
    , CbLumaLevel4x4      = (BLK_Cb|BLK_4x4)
    , CbLumaLevel8x8      = (BLK_Cb|BLK_8x8)
    , CrIntra16x16DCLevel = (BLK_Cr|BLK_16x16|BLK_DC)
    , CrIntra16x16ACLevel = (BLK_Cr|BLK_16x16|BLK_AC)
    , CrLumaLevel4x4      = (BLK_Cr|BLK_4x4)
    , CrLumaLevel8x8      = (BLK_Cr|BLK_8x8)
};

extern const Bs8u SubMbTblP[4][5];
extern const Bs8u SubMbTblB[13][5];
extern const char mbTypeTraceMap[74][16];
extern const char sliceTypeTraceMap[10][3];
extern const char predMode4x4TraceMap[9][20];
extern const char predMode8x8TraceMap[9][20];
extern const char predMode16x16TraceMap[4][12];
extern const char predModeChromaTraceMap[4][12];
extern const char mmcoTraceMap[7][12];
extern const char rplmTraceMap[6][12];
extern const Bs8u Invers4x4Scan[16];

struct NaluTraceMap
{
    static const char map[21][16];
    const char* operator[] (Bs32u i);
};
struct SEIPayloadTraceMap
{
    static const char map[51][38];
    const char* operator[] (Bs32u i);
};
struct ProfileTraceMap
{
    static const char map[13][16];
    const char* operator[] (Bs32u i);
};
struct ARIdcTraceMap
{
    static const char map[19][16];
    const char* operator[] (Bs32u i);
};

inline bool isInter(Bs8u m){ return m >= MBTYPE_P_start; }
inline bool isIntra(Bs8u m){ return m < MBTYPE_P_start; }

inline Bs32u CeilLog2(Bs32s x)
{
    Bs32s size = 0;
    while (x >(1 << size)) size++;
    return size;
}

class Info
{
private:
    std::vector<Bs32u> MbToSliceGroupMap;
    std::vector<MB*>   MbByAddr;
    SPS*   m_prevSPS;
    PPS*   m_prevPPS;
    Slice* m_slice;
    MB*    m_cMB;
    bool   m_firstMB;
    Bs32u  m_prevAddr;

    bool defaultMBFDF(MB& mb);

public:
    Bs32u MaxFrameNum;
    Bs16u MbWidthC;
    Bs16u MbHeightC;
    Bs16u BitDepthY;
    Bs16u BitDepthC;
    Bs16u QpBdOffsetY;
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
    bool isI, isP, isB, isSI, isSP;

    Info(){}
    ~Info(){}

    void newSlice(Slice& s);
    void newMB(MB& mb);
    Bs32u nextMbAddress(Bs32u n);

    inline MB* getMB(Bs32s addr)
    {
        return addr >= 0 && Bs32u(addr) < MbByAddr.size() ? MbByAddr[addr] : 0;
    }

    // assume MB is available
    inline MB& MBRef(Bs32s addr)
    {
        MB* p = getMB(addr);
        if (!p) throw BsReader2::InvalidSyntax();
        return *p;
    }

    // 6.4.8 - 6.4.10
    inline bool  Available(Bs32s addr) { return !(addr < 0 || Bs32u(addr) >= MbByAddr.size() || !MbByAddr[addr]); }
    inline Bs32s mbAddrA(MBLoc& mb) { return (MbaffFrameFlag + 1) * (mb.Addr / (MbaffFrameFlag + 1) - 1); }
    inline Bs32s mbAddrB(MBLoc& mb) { return (MbaffFrameFlag + 1) * (mb.Addr / (MbaffFrameFlag + 1) - PicWidthInMbs); }
    inline Bs32s mbAddrC(MBLoc& mb) { return (MbaffFrameFlag + 1) * (mb.Addr / (MbaffFrameFlag + 1) - PicWidthInMbs + 1); }
    inline Bs32s mbAddrD(MBLoc& mb) { return (MbaffFrameFlag + 1) * (mb.Addr / (MbaffFrameFlag + 1) - PicWidthInMbs - 1); }
    inline bool  AvailableA(MBLoc& mb) { return ((mb.Addr / (MbaffFrameFlag + 1)) % PicWidthInMbs == 0) ? false : Available(mbAddrA(mb)); }
    inline bool  AvailableB(MBLoc& mb) { return Available(mbAddrB(mb)); }
    inline bool  AvailableC(MBLoc& mb) { return ((mb.Addr / (MbaffFrameFlag + 1) + 1) % PicWidthInMbs == 0) ? false : Available(mbAddrC(mb)); }
    inline bool  AvailableD(MBLoc& mb) { return ((mb.Addr / (MbaffFrameFlag + 1)) % PicWidthInMbs == 0) ? false : Available(mbAddrD(mb)); }

    // 6.4.11.1
    inline Bs32s neighbMbA(MB& mb, bool isLuma = true) { return getNeighbLoc(mb, isLuma, -1, 0).Addr; };
    inline Bs32s neighbMbB(MB& mb, bool isLuma = true) { return getNeighbLoc(mb, isLuma, 0, -1).Addr; };

    //6.4.12
    MBLoc getNeighbLoc(MB& mb, bool isLuma, Bs16s xN, Bs16s yN);

    BlkLoc neighb8x8Y(MB& mb, Bs32s luma8x8BlkIdx,   Bs16s xD, Bs16s yD);
    BlkLoc neighb8x8C(MB& mb, Bs32s cx8x8BlkIdx,     Bs16s xD, Bs16s yD);
    BlkLoc neighb4x4Y(MB& mb, Bs32s luma4x4BlkIdx,   Bs16s xD, Bs16s yD);
    BlkLoc neighb4x4C(MB& mb, Bs32s chroma4x4BlkIdx, Bs16s xD, Bs16s yD);

    inline BlkLoc neighb8x8YblkA(MB& mb, Bs32s luma8x8BlkIdx  )
        { return neighb8x8Y(mb, luma8x8BlkIdx,   -1,  0); }; // 6.4.11.2
    inline BlkLoc neighb8x8YblkB(MB& mb, Bs32s luma8x8BlkIdx  )
        { return neighb8x8Y(mb, luma8x8BlkIdx,    0, -1); }; // 6.4.11.2
    inline BlkLoc neighb8x8CblkA(MB& mb, Bs32s chroma8x8BlkIdx)
        { return neighb8x8C(mb, chroma8x8BlkIdx, -1,  0); }; // 6.4.11.3
    inline BlkLoc neighb8x8CblkB(MB& mb, Bs32s chroma8x8BlkIdx)
        { return neighb8x8C(mb, chroma8x8BlkIdx,  0, -1); }; // 6.4.11.3
    inline BlkLoc neighb4x4YblkA(MB& mb, Bs32s luma4x4BlkIdx  )
        { return neighb4x4Y(mb, luma4x4BlkIdx,   -1,  0); }; // 6.4.11.4
    inline BlkLoc neighb4x4YblkB(MB& mb, Bs32s luma4x4BlkIdx  )
        { return neighb4x4Y(mb, luma4x4BlkIdx,    0, -1); }; // 6.4.11.4
    inline BlkLoc neighb4x4CblkA(MB& mb, Bs32s chroma4x4BlkIdx)
        { return neighb4x4C(mb, chroma4x4BlkIdx, -1,  0); }; // 6.4.11.5
    inline BlkLoc neighb4x4CblkB(MB& mb, Bs32s chroma4x4BlkIdx)
        { return neighb4x4C(mb, chroma4x4BlkIdx,  0, -1); }; // 6.4.11.5

    PartLoc neighbPart(MB& mb, Bs8s mbPartIdx, Bs8s subMbPartIdx, Bs16s xD, Bs16s yD);

    inline PartLoc neighbPartA(MB& mb, Bs8s mbPartIdx, Bs8s subMbPartIdx)
        { return neighbPart(mb, mbPartIdx, subMbPartIdx, -1, 0); }; // 6.4.11.7
    inline PartLoc neighbPartB(MB& mb, Bs8s mbPartIdx, Bs8s subMbPartIdx)
        { return neighbPart(mb, mbPartIdx, subMbPartIdx, 0, -1); }; // 6.4.11.7

    inline SPS&   cSPS()   { return *m_prevSPS; }
    inline PPS&   cPPS()   { return *m_prevPPS; }
    inline Slice& cSlice() { return *m_slice; }
    inline MB&    cMB()    { return *m_cMB; }

    Bs8u MbType         (Bs8u mb_type);
    Bs8s MbPartPredMode (MB& mb, Bs8u x);
    Bs8u MbPartWidth    (Bs8u MbType);
    Bs8u MbPartHeight   (Bs8u MbType);
    Bs8u NumMbPart      (MB& mb);

    Bs8u NumSubMbPart   (MB& mb, Bs8u idx);
    Bs8s SubMbPredMode  (MB& mb, Bs8u idx);
    Bs8u SubMbPartWidth (MB& mb, Bs8u idx);
    Bs8u SubMbPartHeight(MB& mb, Bs8u idx);

    Bs8u cbpY(MB& mb);
    Bs8u cbpC(MB& mb);

    Bs8u Intra4x4PredMode(MB& mb, Bs8u idx, bool prev, Bs8u rem);
    Bs8u Intra8x8PredMode(MB& mb, Bs8u idx, bool prev, Bs8u rem);

    inline bool haveResidual(MB& mb) { return cbpY(mb) > 0 || cbpC(mb) > 0 || MbPartPredMode(mb, 0) == Intra_16x16; }
    Bs8u QPy(MB& mb);
};

}