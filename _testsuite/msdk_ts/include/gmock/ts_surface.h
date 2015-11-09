#pragma once

#include "ts_alloc.h"
#include "ts_bitstream.h"

class tsFrameAbstract
{
private:
    mfxU8 m_t;
public:
    virtual mfxU8& Y(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& U(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& V(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& R(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& G(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& B(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU8& A(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return m_t;};
    virtual mfxU16& Y16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return (mfxU16&) m_t;};
    virtual mfxU16& U16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return (mfxU16&) m_t;};
    virtual mfxU16& V16(mfxU32 w, mfxU32 h) {g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR); return (mfxU16&) m_t;};
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

    inline bool isYUV() {return true;};
    inline mfxU8& Y(mfxU32 w, mfxU32 h) { return m_y[h * m_pitch + w]; }
    inline mfxU8& U(mfxU32 w, mfxU32 h) { return m_uv[h / 2 * m_pitch + (w & 0xFFFFFFFE)]; }
    inline mfxU8& V(mfxU32 w, mfxU32 h) { return m_uv[h / 2 * m_pitch + (w & 0xFFFFFFFE) + 1]; }
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
    
    inline bool isYUV() {return true;};
    inline mfxU8& Y(mfxU32 w, mfxU32 h) { return m_y[h * m_pitch + w]; }
    inline mfxU8& U(mfxU32 w, mfxU32 h) { return m_u[h/2 * m_pitch/2 + w/2]; }
    inline mfxU8& V(mfxU32 w, mfxU32 h) { return m_v[h/2 * m_pitch/2 + w/2]; }
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
    
    inline bool isYUV() {return true;};
    inline mfxU8& Y(mfxU32 w, mfxU32 h) { return m_y[h * m_pitch + w * 2]; }
    inline mfxU8& U(mfxU32 w, mfxU32 h) { return m_u[h * m_pitch + (w / 2) * 4]; }
    inline mfxU8& V(mfxU32 w, mfxU32 h) { return m_v[h * m_pitch + (w / 2) * 4]; }
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
    
    inline bool isRGB() {return true;};
    inline mfxU8& R(mfxU32 w, mfxU32 h) { return m_r[h * m_pitch + w * 4]; }
    inline mfxU8& G(mfxU32 w, mfxU32 h) { return m_g[h * m_pitch + w * 4]; }
    inline mfxU8& B(mfxU32 w, mfxU32 h) { return m_b[h * m_pitch + w * 4]; }
    inline mfxU8& A(mfxU32 w, mfxU32 h) { return m_a[h * m_pitch + w * 4]; }
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

    inline bool isYUV16() {return true;};
    inline mfxU16& Y16(mfxU32 w, mfxU32 h) { return m_y16[h * m_pitch/2 + w]; }
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

    inline mfxU8& Y(mfxU32 w, mfxU32 h) { return m_pFrame->Y(w, h); };
    inline mfxU8& U(mfxU32 w, mfxU32 h) { return m_pFrame->U(w, h); };
    inline mfxU8& V(mfxU32 w, mfxU32 h) { return m_pFrame->V(w, h); };
    inline mfxU8& R(mfxU32 w, mfxU32 h) { return m_pFrame->R(w, h); };
    inline mfxU8& G(mfxU32 w, mfxU32 h) { return m_pFrame->G(w, h); };
    inline mfxU8& B(mfxU32 w, mfxU32 h) { return m_pFrame->B(w, h); };
    inline mfxU8& A(mfxU32 w, mfxU32 h) { return m_pFrame->A(w, h); };
    inline mfxU16& Y16(mfxU32 w, mfxU32 h) { return m_pFrame->Y16(w, h); };
    inline mfxU16& U16(mfxU32 w, mfxU32 h) { return m_pFrame->U16(w, h); };
    inline mfxU16& V16(mfxU32 w, mfxU32 h) { return m_pFrame->V16(w, h); };
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
