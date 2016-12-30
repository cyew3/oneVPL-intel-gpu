//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_config.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_common.h"
#include "mfx_h265_encode_hw_set.h"
#include "mfx_h265_encode_hw_utils.h"
#include <vector>
#include <exception>

namespace MfxHwH265Encode
{

class EndOfBuffer : public std::exception
{
public:
    EndOfBuffer() : std::exception() {}
};

class BitstreamWriter
{
public:
    BitstreamWriter(mfxU8* bs, mfxU32 size, mfxU8 bitOffset = 0);
    ~BitstreamWriter();

    void PutBits        (mfxU32 n, mfxU32 b);
    void PutBitsBuffer  (mfxU32 n, void* b, mfxU32 offset = 0);
    void PutBit         (mfxU32 b);
    void PutGolomb      (mfxU32 b);
    void PutTrailingBits(bool bCheckAligned = false);

    inline void PutUE   (mfxU32 b)    { PutGolomb(b); }
    inline void PutSE   (mfxI32 b)    { (b > 0) ? PutGolomb((b<<1)-1) : PutGolomb((-b)<<1); }

    inline mfxU32 GetOffset() { return (mfxU32)(m_bs - m_bsStart) * 8 + m_bitOffset - m_bitStart; }
    inline mfxU8* GetStart () { return m_bsStart; }

    void Reset(mfxU8* bs = 0, mfxU32 size = 0, mfxU8 bitOffset = 0);

private:
    mfxU8* m_bsStart;
    mfxU8* m_bsEnd;
    mfxU8* m_bs;
    mfxU8  m_bitStart;
    mfxU8  m_bitOffset;
};

class BitstreamReader
{
public:
    BitstreamReader(mfxU8* bs, mfxU32 size, mfxU8 bitOffset = 0);
    ~BitstreamReader();

    mfxU32 GetBit ();
    mfxU32 GetBits(mfxU32 n);
    mfxU32 GetUE  ();
    mfxI32 GetSE  ();

    inline mfxU32 GetOffset() { return (mfxU32)(m_bs - m_bsStart) * 8 + m_bitOffset - m_bitStart; }
    inline mfxU8* GetStart() { return m_bsStart; }

    inline void SetEmulation(bool f) { m_emulation = f; };
    inline bool GetEmulation() { return m_emulation; };

    void Reset(mfxU8* bs = 0, mfxU32 size = 0, mfxU8 bitOffset = 0);

private:
    mfxU8* m_bsStart;
    mfxU8* m_bsEnd;
    mfxU8* m_bs;
    mfxU8  m_bitStart;
    mfxU8  m_bitOffset;
    bool   m_emulation;
};

class HeaderReader
{
public:
    HeaderReader(){};
    ~HeaderReader(){};

    static mfxStatus ReadNALU(BitstreamReader& bs, NALU & nalu);
    static mfxStatus ReadSPS (BitstreamReader& bs, SPS  & sps);
    static mfxStatus ReadPPS (BitstreamReader& bs, PPS  & pps);

};

class HeaderPacker
{
public:

    HeaderPacker();
    ~HeaderPacker();

    mfxStatus Reset(MfxVideoParam const & par);

    inline void GetAUD(mfxU8*& buf, mfxU32& len, mfxU8 pic_type){assert(pic_type < 3); buf = m_bs_aud[pic_type%3]; len = 7;} 
    inline void GetVPS(mfxU8*& buf, mfxU32& len){buf = m_bs_vps; len = m_sz_vps;}  
    inline void GetSPS(mfxU8*& buf, mfxU32& len){buf = m_bs_sps; len = m_sz_sps;}
    inline void GetPPS(mfxU8*& buf, mfxU32& len){buf = m_bs_pps; len = m_sz_pps;}
    void GetPrefixSEI(Task const & task, mfxU8*& buf, mfxU32& len);
    void GetSuffixSEI(Task const & task, mfxU8*& buf, mfxU32& len);
    void GetSSH(Task const & task, mfxU32 id, mfxU8*& buf, mfxU32& len, mfxU32* qpd_offset = 0);

    static void PackNALU (BitstreamWriter& bs, NALU  const &  nalu);
    static void PackAUD  (BitstreamWriter& bs, mfxU8 pic_type);
    static void PackVPS  (BitstreamWriter& bs, VPS   const &  vps);
    static void PackSPS  (BitstreamWriter& bs, SPS   const &  sps);
    static void PackPPS  (BitstreamWriter& bs, PPS   const &  pps);
    static void PackSSH  (BitstreamWriter& bs, 
                          NALU  const &     nalu, 
                          SPS   const &     sps,
                          PPS   const &     pps,
                          Slice const &     slice,
                          mfxU32* qpd_offset = 0);
    static void PackVUI  (BitstreamWriter& bs, VUI        const & vui, mfxU16 max_sub_layers_minus1);
    static void PackHRD  (BitstreamWriter& bs, HRDInfo    const & hrd, bool commonInfPresentFlag, mfxU16 maxNumSubLayersMinus1);
    static void PackPTL  (BitstreamWriter& bs, LayersInfo const & ptl, mfxU16 max_sub_layers_minus1);
    static void PackSLO  (BitstreamWriter& bs, LayersInfo const & slo, mfxU16 max_sub_layers_minus1);
    static void PackSTRPS(BitstreamWriter& bs, const STRPS * h, mfxU32 num, mfxU32 idx);

    static void PackSEIPayload(BitstreamWriter& bs, VUI const & vui, BufferingPeriodSEI const & bp);
    static void PackSEIPayload(BitstreamWriter& bs, VUI const & vui, PicTimingSEI const & pt);

    static mfxStatus PackRBSP(mfxU8* dst, mfxU8* src, mfxU32& dst_size, mfxU32 src_size);


private:
    static const mfxU32 RBSP_SIZE   = 1024;
    static const mfxU32 AUD_BS_SIZE = 8;
    static const mfxU32 VPS_BS_SIZE = 256;
    static const mfxU32 SPS_BS_SIZE = 256;
    static const mfxU32 PPS_BS_SIZE = 128;
    static const mfxU32 SSH_BS_SIZE = MAX_SLICES * 16;

    mfxU8  m_rbsp[RBSP_SIZE];
    mfxU8  m_bs_aud[3][AUD_BS_SIZE];
    mfxU8  m_bs_vps[VPS_BS_SIZE];
    mfxU8  m_bs_sps[SPS_BS_SIZE];
    mfxU8  m_bs_pps[PPS_BS_SIZE];
    mfxU8  m_bs_ssh[SSH_BS_SIZE];
    mfxU32 m_sz_vps;
    mfxU32 m_sz_sps;
    mfxU32 m_sz_pps;
    mfxU32 m_sz_ssh;

    const MfxVideoParam * m_par;
    BitstreamWriter       m_bs;
};

} //MfxHwH265Encode

#endif
