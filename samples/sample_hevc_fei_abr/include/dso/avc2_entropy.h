#pragma once

#include "avc2_info.h"
#include "h264_cavlc_tables.h"

#define BS_AVC2_ADE_MODE 1 //0 - literal standard implementation, 1 - optimized(2-byte buffering affects SE offsets in trace)

namespace BS_AVC2
{

class ADE
{
private:
    BsReader2::Reader& m_bs;
    Bs64u m_bpos;
    bool  m_pcm;
#if (BS_AVC2_ADE_MODE == 1)
    Bs32u m_val;
    Bs32u m_range;
    Bs32s m_bits;

    inline Bs32u B(Bs16u n) { m_bpos += n * 8; return m_bs.GetBytes(n, true); }
#else
    Bs16u m_codIRange;
    Bs16u m_codIOffset;

    inline Bs16u b(Bs16u n) { m_bpos += n; return m_bs.GetBits(n); }
    inline Bs16u b() { m_bpos++; return m_bs.GetBit(); }
#endif

public:
    ADE(BsReader2::Reader& r) : m_bs(r), m_pcm(false) {}
    ~ADE(){}

    inline void Init()
    {
#if (BS_AVC2_ADE_MODE == 1)
        m_val = B(3);
        m_range = 510;
        m_bits = 15;

        if (!m_pcm)
            m_bpos = 15;
#else
        m_codIRange = 510;
        m_codIOffset = b(9);

        if (!m_pcm)
            m_bpos = 0;
#endif
        m_pcm = false;
    }

    Bs8u DecodeDecision(Bs8u& ctxState);
    Bs8u DecodeBypass();
    Bs8u DecodeTerminate();

#if (BS_AVC2_ADE_MODE == 1)
    inline Bs16u GetR() { return Bs16u(m_range); }
    inline Bs16u GetV() { return Bs16u((m_val >> (m_bits))); }
    inline Bs64u BitCount() { return m_bpos - m_bits; }
    inline void  InitPCMBlock() { m_bs.ReturnBits(m_bits); m_bpos -= m_bits; m_bits = 0; m_pcm = true; }
#else
    inline Bs16u GetR() { return m_codIRange; }
    inline Bs16u GetV() { return m_codIOffset; }
    inline Bs64u BitCount() { return m_bpos; }
    inline void  InitPCMBlock() { m_pcm = true; };
#endif

};

class CABAC : private ADE
{
private:
    Bs8u    m_ctxState[1024];
    Info*   m_info;

    inline Bs8u DecodeBin(Bs16s off, Bs16s inc)
    {
        BinCount++;
        if (off < 0)   inc = off;
        if (inc == -1) return DecodeBypass();
        if (inc == -2) return DecodeTerminate();
        if (inc < 0)   throw BsReader2::InvalidSyntax();
        return DecodeDecision(m_ctxState[off + inc]);
    }

public:
    Bs64u BinCount;

    CABAC(BsReader2::Reader& r, Info* inf)
        : ADE(r)
        , m_info(inf)
    {}
    ~CABAC() {}

    void Init(Slice& s);
    inline void InitADE() { ADE::Init(); }
    inline void InitPCMBlock() { ADE::InitPCMBlock(); }

    inline Bs16u GetR() { return ADE::GetR(); }
    inline Bs16u GetV() { return ADE::GetV(); }
    inline Bs64u BitCount() { return ADE::BitCount(); }

    inline bool endOfSliceFlag() { return !!DecodeBin(-2, 0); }
    bool  mbSkipFlag();
    bool  mbFieldDecFlag();
    Bs8u  mbType();
    bool  transformSize8x8Flag();
    Bs8u  CBP();
    Bs8s  mbQpDelta();
    Bs8u  subMbType();
    Bs8u  refIdxLX(Bs16u mbPartIdx, bool X);
    Bs16s mvdLX(Bs16u mbPartIdx, Bs16u subMbPartIdx, bool compIdx, bool X);
    bool  prevIntraPredModeFlag();
    Bs8u  remIntraPredMode();
    Bs8u  intraChromaPredMode();
    bool  codedBlockFlag(Bs8u blkIdx, Bs8u ctxBlockCat, bool isCb);
    bool  SCF(Bs8u ctxBlockCat, Bs8u levelListIdx);
    bool  LSCF(Bs8u ctxBlockCat, Bs8u levelListIdx);
    Bs16u coeffAbsLevel(Bs8u ctxBlockCat, Bs8u gt1, Bs8u eq1);
    bool  coeffSignFlag();
};

inline Bs16u TrailingOnes(Bs16u coeff_token ) { return (coeff_token >> 5); };
inline Bs16u TotalCoeff  (Bs16u coeff_token ) { return (coeff_token & 0x1F); };
  
class CAVLC
{
private:
    BsReader2::Reader& m_bs;
    Info*              m_info;

    inline Bs16u b(Bs16u n) { return m_bs.GetBits(n); }
    inline Bs16u b() { return m_bs.GetBit(); }

    Bs16u vlc16(const Bs16u* vlcTable);

public:
    CAVLC(BsReader2::Reader& r, Info* inf)
        : m_bs(r)
        , m_info(inf)
    {}
    ~CAVLC(){}

    Bs16u CoeffToken(Bs8u blkType, Bs8u blkIdx, bool isCb);
    Bs16u LevelPrefix();
    Bs16u LevelSuffix(Bs16u suffixLength, Bs16u level_prefix);
    Bs16u TotalZeros(Bs16u maxNumCoeff, Bs16u tzVlcIndex);
    inline Bs16u RunBefore(Bs16u zerosLeft) { return vlc16(vlcRunBefore[BS_MIN(7, zerosLeft)]); }
    inline bool TrailingOnesSign() { return !!b(); }
    Bs8u  CBP();
};

}