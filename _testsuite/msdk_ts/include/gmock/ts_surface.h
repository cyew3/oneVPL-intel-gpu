/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "ts_alloc.h"
#include "ts_bitstream.h"

template<class U>
class tsSampleAbstract
{
public:
    virtual ~tsSampleAbstract() {}
    virtual operator U() = 0;
    virtual U operator=(U l) = 0;
    inline U operator=(tsSampleAbstract<U>& l) { return (*this = (U)l); }
};

template<class U>
class tsSample : public tsSampleAbstract<U>
{
public:
    tsSampleAbstract<U>* m_pImpl;

    tsSample(tsSampleAbstract<U>* pImpl)
        : m_pImpl(pImpl)
    {}
    ~tsSample() { delete m_pImpl; }
    inline operator U() { return (U)*m_pImpl; }
    inline U operator=(U l) { return (*m_pImpl = l);  }
    inline U operator=(tsSample<U>& l) { return (*this = (U)l); }
};

template<class U, class MaxU>
class tsSampleImpl : public tsSampleAbstract<U>
{
private:
    MaxU* m_p;
    MaxU  m_mask;
    mfxU16 m_shift;
public:
    tsSampleImpl(void* p, MaxU mask = (MaxU(-1) >> ((sizeof(MaxU) - sizeof(U)) * 8)), mfxU16 shift = 0)
        : m_p((MaxU*)p)
        , m_mask(mask)
        , m_shift(shift)
    {}
    inline operator U() { return U((m_p[0] & m_mask) >> m_shift); }
    inline U operator=(U l)
    {
        m_p[0] &= ~m_mask;
        m_p[0] |= ((l << m_shift) & m_mask);
        return l;
    }
};

class tsFrameAbstract
{
public:
    virtual ~tsFrameAbstract() { }

    virtual tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> U(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> V(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> R(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> G(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> B(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU8> A(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);    return tsSample<mfxU8>(0);};
    virtual tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return tsSample<mfxU16>(0);};
    virtual tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return tsSample<mfxU16>(0);};
    virtual tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return tsSample<mfxU16>(0);};
    virtual bool isYUV() {return false;};
    virtual bool isYUV16() {return false;};
    virtual bool isRGB() {return false;};
};

class tsFrameNV12 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU8* m_y;
    mfxU8* m_uv;
public:
    tsFrameNV12(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y)
        , m_uv(d.UV)
    {}

    virtual ~tsFrameNV12() { }

    inline bool isYUV() {return true;};
    inline tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_y[h * m_pitch + w])); }
    inline tsSample<mfxU8> U(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_uv[h / 2 * m_pitch + (w & 0xFFFFFFFE)])); }
    inline tsSample<mfxU8> V(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_uv[h / 2 * m_pitch + (w & 0xFFFFFFFE) + 1])); }
};

class tsFrameYV12 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU8* m_y;
    mfxU8* m_u;
    mfxU8* m_v;
public:
    tsFrameYV12(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y)
        , m_u(d.U)
        , m_v(d.V)
    {}

    virtual ~tsFrameYV12() { }

    inline bool isYUV() {return true;};
    inline tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_y[h * m_pitch + w])); }
    inline tsSample<mfxU8> U(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_u[h/2 * m_pitch/2 + w/2])); }
    inline tsSample<mfxU8> V(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_v[h/2 * m_pitch/2 + w/2])); }
};

class tsFrameYUY2 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU8* m_y;
    mfxU8* m_u;
    mfxU8* m_v;
public:
    tsFrameYUY2(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y)
        , m_u(d.U)
        , m_v(d.V)
    {}

    virtual ~tsFrameYUY2() { }

    inline bool isYUV() {return true;};
    inline tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_y[h * m_pitch + w * 2])); }
    inline tsSample<mfxU8> U(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_u[h * m_pitch + (w / 2) * 4])); }
    inline tsSample<mfxU8> V(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_v[h * m_pitch + (w / 2) * 4])); }
};

class tsFrameAYUV : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU8* m_y;
    mfxU8* m_u;
    mfxU8* m_v;
public:
    tsFrameAYUV(mfxFrameData d)
        : m_pitch(mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y)
        , m_u(d.U)
        , m_v(d.V)
    {}

    virtual ~tsFrameAYUV() { }

    inline bool isYUV() { return true; };
    inline tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_y[h * m_pitch + w * 4])); }
    inline tsSample<mfxU8> U(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_u[h * m_pitch + w * 4])); }
    inline tsSample<mfxU8> V(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_v[h * m_pitch + w * 4])); }
};

class tsFrameP010 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU16* m_y;
    mfxU16* m_uv;
public:
    tsFrameP010(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y16)
        , m_uv(d.U16)
    {}

    virtual ~tsFrameP010() { }

    inline bool isYUV16() {return true;};
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_y[h * (m_pitch/2) + w], 0xffc0, 6)); }
    inline tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_uv[(h/2) * (m_pitch/2) + (w%2==0?w:(w-1))], 0xffc0, 6)); }
    inline tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_uv[(h/2) * (m_pitch/2) + (w%2==0?w:(w-1)) + 1], 0xffc0, 6)); }
};

class tsFrameP010s0 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU16* m_y;
    mfxU16* m_uv;
public:
    tsFrameP010s0(mfxFrameData d)
        : m_pitch(mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y(d.Y16)
        , m_uv(d.U16)
    {}

    virtual ~tsFrameP010s0() { }

    inline bool isYUV16() { return true; };
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_y[h * (m_pitch / 2) + w])); }
    inline tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_uv[(h / 2) * (m_pitch / 2) + (w % 2 == 0 ? w : (w - 1))])); }
    inline tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_uv[(h / 2) * (m_pitch / 2) + (w % 2 == 0 ? w : (w - 1)) + 1])); }
};

class tsFrameY210 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU16* m_y;
    mfxU16* m_u;
    mfxU16* m_v;
public:
    tsFrameY210(mfxFrameData d)
        : m_pitch((mfxU32(d.PitchHigh << 16) + d.PitchLow) / 2)
        , m_y(d.Y16)
        , m_u(d.U16)
        , m_v(d.V16)
    {}

    virtual ~tsFrameY210() { }

    inline bool isYUV16() { return true; };
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_y[h * m_pitch + w * 2], 0xffc0, 6)); }
    inline tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_u[h * m_pitch + (w / 2) * 4], 0xffc0, 6)); }
    inline tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_v[h * m_pitch + (w / 2) * 4], 0xffc0, 6)); }
};

class tsFrameY410 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxY410* m_y;
public:
    tsFrameY410(mfxFrameData d)
        : m_pitch((mfxU32(d.PitchHigh << 16) + d.PitchLow) / 4)
        , m_y(d.Y410)
    {}

    virtual ~tsFrameY410() { }

    inline bool isYUV16() { return true; };
    inline tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU32>(&m_y[h * m_pitch + w], 0x000003ff,  0)); }
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU32>(&m_y[h * m_pitch + w], 0x000ffc00, 10)); }
    inline tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU32>(&m_y[h * m_pitch + w], 0x3ff00000, 20)); }
};

class tsFrameRGB4 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU8* m_r;
    mfxU8* m_g;
    mfxU8* m_b;
    mfxU8* m_a;
public:
    tsFrameRGB4(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_r(d.R)
        , m_g(d.G)
        , m_b(d.B)
        , m_a(d.A)
    {}

    virtual ~tsFrameRGB4() { }

    inline bool isRGB() {return true;};
    inline tsSample<mfxU8> R(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_r[h * m_pitch + w * 4])); }
    inline tsSample<mfxU8> G(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_g[h * m_pitch + w * 4])); }
    inline tsSample<mfxU8> B(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_b[h * m_pitch + w * 4])); }
    inline tsSample<mfxU8> A(mfxU32 w, mfxU32 h) { return tsSample<mfxU8>(new tsSampleImpl<mfxU8, mfxU8>(&m_a[h * m_pitch + w * 4])); }
};

class tsFrameR16 : public tsFrameAbstract
{
private:
    mfxU32 m_pitch;
    mfxU16* m_y16;
public:
    tsFrameR16(mfxFrameData d)
        : m_pitch( mfxU32(d.PitchHigh << 16) + d.PitchLow)
        , m_y16(d.Y16)
    {}

    virtual ~tsFrameR16() { }

    inline bool isYUV16() {return true;};
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return tsSample<mfxU16>(new tsSampleImpl<mfxU16, mfxU16>(&m_y16[h * m_pitch/2 + w])); }
};

class tsFrame
{
private:
    tsFrameAbstract* m_pFrame;
public:
    mfxFrameInfo m_info;

    tsFrame(mfxFrameSurface1);
    ~tsFrame();

    tsFrame& operator=(tsFrame&);

    inline tsSample<mfxU8> Y(mfxU32 w, mfxU32 h) { return m_pFrame->Y(w, h); };
    inline tsSample<mfxU8> U(mfxU32 w, mfxU32 h) { return m_pFrame->U(w, h); };
    inline tsSample<mfxU8> V(mfxU32 w, mfxU32 h) { return m_pFrame->V(w, h); };
    inline tsSample<mfxU8> R(mfxU32 w, mfxU32 h) { return m_pFrame->R(w, h); };
    inline tsSample<mfxU8> G(mfxU32 w, mfxU32 h) { return m_pFrame->G(w, h); };
    inline tsSample<mfxU8> B(mfxU32 w, mfxU32 h) { return m_pFrame->B(w, h); };
    inline tsSample<mfxU8> A(mfxU32 w, mfxU32 h) { return m_pFrame->A(w, h); };
    inline tsSample<mfxU16> Y16(mfxU32 w, mfxU32 h) { return m_pFrame->Y16(w, h); };
    inline tsSample<mfxU16> U16(mfxU32 w, mfxU32 h) { return m_pFrame->U16(w, h); };
    inline tsSample<mfxU16> V16(mfxU32 w, mfxU32 h) { return m_pFrame->V16(w, h); };
    inline bool isYUV() {return m_pFrame->isYUV(); };
    inline bool isYUV16() {return m_pFrame->isYUV16(); };
    inline bool isRGB() {return m_pFrame->isRGB(); };
};


class tsSurfaceProcessor
{
public:
    mfxU32 m_max;
    mfxU32 m_cur;
    bool   m_eos;

    tsSurfaceProcessor(mfxU32 n_frames = 0xFFFFFFFF);
    virtual ~tsSurfaceProcessor() {}

    virtual mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
    virtual mfxStatus ProcessSurface(mfxFrameSurface1&) { g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return MFX_ERR_UNDEFINED_BEHAVIOR; };
};


class tsNoiseFiller : public tsSurfaceProcessor
{
public:
    tsNoiseFiller(mfxU32 n_frames = 0xFFFFFFFF);
    ~tsNoiseFiller();

    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};


class tsRawReader : public tsSurfaceProcessor, tsReader
{
private:
    mfxFrameSurface1 m_surf;
    mfxU32           m_fsz;
    mfxU8*           m_buf;

    void Init(mfxFrameInfo fi);
public:
    bool m_disable_shift_hack;

    tsRawReader(const char* fname, mfxFrameInfo fi, mfxU32 n_frames = 0xFFFFFFFF);
    tsRawReader(mfxBitstream bs, mfxFrameInfo fi, mfxU32 n_frames = 0xFFFFFFFF);
    ~tsRawReader();

    mfxStatus ResetFile();

    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

class tsSurfaceWriter : public tsSurfaceProcessor
{
private:
    FILE* m_file;
public:
    tsSurfaceWriter(const char* fname);
    ~tsSurfaceWriter();
    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

class tsAutoFlushFiller : public tsSurfaceProcessor
{
public:
    tsAutoFlushFiller(mfxU32 n_frames = 0xFFFFFFFF) : tsSurfaceProcessor(n_frames) {};
    ~tsAutoFlushFiller() {};

    mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        if (m_eos)
            return 0;

        if (m_cur++ >= m_max)
        {
            m_eos = true;
            return 0;
        }

        return ps;
    }
};

mfxF64 PSNR(tsFrame& ref, tsFrame& src, mfxU32 id);
mfxF64 SSIM(tsFrame& ref, tsFrame& src, mfxU32 id);

bool operator == (const mfxFrameSurface1& s1, const mfxFrameSurface1& s2);
bool operator != (const mfxFrameSurface1& s1, const mfxFrameSurface1& s2);
