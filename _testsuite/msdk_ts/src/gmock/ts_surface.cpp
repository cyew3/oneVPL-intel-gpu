/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_surface.h"
#include "math.h"

tsFrame::tsFrame(mfxFrameSurface1 s)
    : m_pFrame(0)
    , m_info(s.Info)
{
    switch(s.Info.FourCC)
    {
    case MFX_FOURCC_NV12: m_pFrame = new tsFrameNV12(s.Data); break;
    case MFX_FOURCC_YV12: m_pFrame = new tsFrameYV12(s.Data); break;
    case MFX_FOURCC_YUY2: m_pFrame = new tsFrameYUY2(s.Data); break;
    case MFX_FOURCC_BGR4: std::swap(s.Data.B, s.Data.R);
    case MFX_FOURCC_RGB4: m_pFrame = new tsFrameRGB4(s.Data); break;
    case MFX_FOURCC_R16:  m_pFrame = new tsFrameR16(s.Data); break;
        break;
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }
}

tsFrame::~tsFrame()
{
    if(m_pFrame)
    {
        delete m_pFrame;
    }
}

inline mfxU32 NumComponents(mfxFrameInfo fi)
{
    if(    fi.FourCC == MFX_FOURCC_RGB4
        || fi.FourCC == MFX_FOURCC_BGR4)
    {
        return 4;
    }

    if(fi.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
    {
        return 1;
    }

    return 3;
}

tsFrame& tsFrame::operator=(tsFrame& src)
{
    mfxU32 n = TS_MIN(NumComponents(m_info), NumComponents(src.m_info));
    mfxU32 maxw = TS_MIN(m_info.Width, src.m_info.Width);
    mfxU32 maxh = TS_MIN(m_info.Height, src.m_info.Height);

    if(    m_info.CropW 
        && m_info.CropH
        && src.m_info.CropW 
        && src.m_info.CropH)
    {
        maxw = TS_MIN(m_info.CropW, src.m_info.CropW);
        maxh = TS_MIN(m_info.CropH, src.m_info.CropH);
    }

    if(    src.m_info.Width > m_info.Width
        || src.m_info.Height > m_info.Height)
    {
        g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if(isYUV() && src.isYUV())
    {
        for(mfxU32 h = 0; h < maxh; h ++)
        {
            for(mfxU32 w = 0; w < maxw; w ++)
            {
                Y(w + m_info.CropX, h + m_info.CropY) = src.Y(w + src.m_info.CropX, h + src.m_info.CropY);
                if(n == 3)
                {
                    U(w + m_info.CropX, h + m_info.CropY) = src.U(w + src.m_info.CropX, h + src.m_info.CropY);
                    V(w + m_info.CropX, h + m_info.CropY) = src.V(w + src.m_info.CropX, h + src.m_info.CropY);
                }
            }
        }
    }
    else if(isYUV16() && src.isYUV16())
    {
        for(mfxU32 h = 0; h < maxh; h ++)
        {
            for(mfxU32 w = 0; w < maxw; w ++)
            {
                Y16(w + m_info.CropX, h + m_info.CropY) = src.Y16(w + src.m_info.CropX, h + src.m_info.CropY);
                if(n == 3)
                {
                    U16(w + m_info.CropX, h + m_info.CropY) = src.U16(w + src.m_info.CropX, h + src.m_info.CropY);
                    V16(w + m_info.CropX, h + m_info.CropY) = src.V16(w + src.m_info.CropX, h + src.m_info.CropY);
                }
            }
        }
    }
    else if(isRGB() && src.isRGB())
    {
        for(mfxU32 h = 0; h < maxh; h ++)
        {
            for(mfxU32 w = 0; w < maxw; w ++)
            {
                R(w + m_info.CropX, h + m_info.CropY) = src.R(w + src.m_info.CropX, h + src.m_info.CropY);
                G(w + m_info.CropX, h + m_info.CropY) = src.G(w + src.m_info.CropX, h + src.m_info.CropY);
                B(w + m_info.CropX, h + m_info.CropY) = src.B(w + src.m_info.CropX, h + src.m_info.CropY);
                A(w + m_info.CropX, h + m_info.CropY) = src.A(w + src.m_info.CropX, h + src.m_info.CropY);
            }
        }
    }
    else
    {
        g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return *this;
}

tsSurfaceProcessor::tsSurfaceProcessor(mfxU32 n_frames) 
    : m_max(n_frames)
    , m_cur(0)
    , m_eos(false)
{
}

mfxFrameSurface1* tsSurfaceProcessor::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
{
    if(ps)
    {
        if(m_cur++ >= m_max)
        {
            return 0;
        }
        bool useAllocator = pfa && !ps->Data.Y;

        if(useAllocator)
        {
            TRACE_FUNC3(mfxFrameAllocator::Lock, pfa->pthis, ps->Data.MemId, &(ps->Data));
            g_tsStatus.check(pfa->Lock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
        }

        TRACE_FUNC1(tsSurfaceProcessor::ProcessSurface, *ps);
        g_tsStatus.check(ProcessSurface(*ps));

        if(useAllocator)
        {
            TRACE_FUNC3(mfxFrameAllocator::Unlock, pfa->pthis, ps->Data.MemId, &(ps->Data));
            g_tsStatus.check(pfa->Unlock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
        }
    }

    if(m_eos)
    {
        m_max = m_cur;
        return 0;
    }

    return ps;
}

tsNoiseFiller::tsNoiseFiller(mfxU32 n_frames) 
    : tsSurfaceProcessor(n_frames)
{
}

tsNoiseFiller::~tsNoiseFiller()
{
}

mfxStatus tsNoiseFiller::ProcessSurface(mfxFrameSurface1& s)
{
    tsFrame d(s);

    if(d.isYUV())
    {
        for(mfxU32 h = 0; h < s.Info.Height; h++)
        {
            for(mfxU32 w = 0; w < s.Info.Width; w++)
            {
                d.Y(w,h) = rand() % (1 << 8);
                d.U(w,h) = rand() % (1 << 8);
                d.V(w,h) = rand() % (1 << 8);
            }
        }
    }
    else if(d.isRGB())
    {
        for(mfxU32 h = 0; h < s.Info.Height; h++)
        {
            for(mfxU32 w = 0; w < s.Info.Width; w++)
            {
                d.R(w,h) = rand() % (1 << 8);
                d.G(w,h) = rand() % (1 << 8);
                d.B(w,h) = rand() % (1 << 8);
                d.A(w,h) = rand() % (1 << 8);
            }
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

tsRawReader::tsRawReader(const char* fname, mfxFrameInfo fi, mfxU32 n_frames)
    : tsSurfaceProcessor(n_frames)
    , tsReader(fname)
    , m_surf()
    , m_fsz(0)
    , m_buf(0)
{
    Init(fi);
}

tsRawReader::tsRawReader(mfxBitstream bs, mfxFrameInfo fi, mfxU32 n_frames)
    : tsSurfaceProcessor(n_frames)
    , tsReader(bs)
    , m_surf()
    , m_fsz(0)
    , m_buf(0)
{
    Init(fi);
}

void tsRawReader::Init(mfxFrameInfo fi)
{
    //no crops for internal surface
    if(fi.CropW) fi.Width  = fi.CropW;
    if(fi.CropH) fi.Height = fi.CropH;
    fi.CropX = fi.CropY = 0;

    mfxU32 fsz = fi.Width * fi.Height;
    mfxU32 pitch = 0;
    mfxFrameData& m_data = m_surf.Data;

    m_surf.Info = fi;

    switch(fi.ChromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV400 : m_fsz = fsz; break;
    case MFX_CHROMAFORMAT_YUV420 : m_fsz = fsz * 3 / 2; break;
    case MFX_CHROMAFORMAT_YUV422 : 
    case MFX_CHROMAFORMAT_YUV422V: m_fsz = fsz * 2; break;
    case MFX_CHROMAFORMAT_YUV444 : m_fsz = fsz * 4; break;
    case MFX_CHROMAFORMAT_YUV411 : 
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }

    if(MFX_FOURCC_R16 == m_surf.Info.FourCC)
        m_fsz = m_fsz * 2;

    m_buf = new mfxU8[m_fsz];

    switch(fi.FourCC)
    {
    case MFX_FOURCC_NV12:
        m_data.Y     = m_buf;
        m_data.UV    = m_data.Y + fsz;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_YV12:
        m_data.Y     = m_buf;
        m_data.U     = m_data.Y + fsz;
        m_data.V     = m_data.U + fsz / 4;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_YUY2:
        m_data.Y     = m_buf;
        m_data.U     = m_data.Y + 1;
        m_data.V     = m_data.U + 3;
        pitch        = fi.Width * 2;
        break;
    case MFX_FOURCC_RGB4:
        m_data.R     = m_buf;
        m_data.G     = m_data.R + fsz;
        m_data.B     = m_data.G + fsz;
        m_data.A     = m_data.B + fsz;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_BGR4:
        m_data.B     = m_buf;
        m_data.G     = m_data.B + fsz;
        m_data.R     = m_data.G + fsz;
        m_data.A     = m_data.R + fsz;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_R16:
        m_data.Y16   = (mfxU16*) m_buf;
        pitch        = fi.Width * 2;
        break;
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }

    m_data.PitchLow  = (pitch & 0xFFFF);
    m_data.PitchHigh = (pitch >> 16);
}

tsRawReader::~tsRawReader()
{
    if(m_buf)
    {
        delete[] m_buf;
    }
}

mfxStatus tsRawReader::ProcessSurface(mfxFrameSurface1& s)
{
    m_eos = (m_fsz != Read(m_buf, m_fsz));

    if(!m_eos)
    {
        tsFrame src(m_surf);
        tsFrame dst(s);

        dst = src;
        s.Data.FrameOrder = m_cur++;
    }

    return MFX_ERR_NONE;
}

mfxStatus tsRawReader::ResetFile()
{
    return SeekToStart();
}

tsSurfaceWriter::tsSurfaceWriter(const char* fname)
    : m_file(0)
{
#pragma warning(disable:4996)
    m_file = fopen(fname, "wb");  
#pragma warning(default:4996)
}

tsSurfaceWriter::~tsSurfaceWriter()
{
    if(m_file)
    {
        fclose(m_file);
    }
}

mfxStatus tsSurfaceWriter::ProcessSurface(mfxFrameSurface1& s)
{
    mfxU32 pitch = (s.Data.PitchHigh << 16) + s.Data.PitchLow;

    if(s.Info.FourCC == MFX_FOURCC_NV12)
    {
        for(mfxU16 i = s.Info.CropY; i < (s.Info.CropH + s.Info.CropY); i ++)
        {
            fwrite(s.Data.Y + pitch * i + s.Info.CropX, 1, s.Info.CropW, m_file);
        }
    
        for(mfxU16 i = (s.Info.CropY / 2); i < ((s.Info.CropH + s.Info.CropY) / 2); i ++)
        {
            fwrite(s.Data.UV + pitch * i + s.Info.CropX, 1, s.Info.CropW, m_file);
        }
    }
    else if(s.Info.FourCC == MFX_FOURCC_RGB4)
    {
        mfxU8* ptr = 0;
        ptr = TS_MIN( TS_MIN(s.Data.R, s.Data.G), s.Data.B );
        ptr = ptr + s.Info.CropX + s.Info.CropY * pitch;

        for(mfxU32 i = s.Info.CropY; i < s.Info.CropH; i++) 
        {
            fwrite(ptr + i * pitch, 1, 4*s.Info.CropW, m_file);
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxF64 PSNR(tsFrame& ref, tsFrame& src, mfxU32 id)
{
    mfxF64 max  = (1 << 8) - 1;
    mfxF64 size = ref.m_info.CropW * ref.m_info.CropH;
    mfxI32 diff = 0;
    mfxU64 dist = 0;
    mfxU32 chroma_step = 1;
    mfxU32 maxw = TS_MIN(ref.m_info.CropW, src.m_info.CropW);
    mfxU32 maxh = TS_MIN(ref.m_info.CropH, src.m_info.CropH);

#define GET_DIST(COMPONENT, STEP)                                              \
    for(mfxU32 y = 0; y < maxh; y += STEP)                                     \
    {                                                                          \
        for(mfxU32 x = 0; x < maxw; x += STEP)                                 \
        {                                                                      \
            diff = ref.COMPONENT(x + ref.m_info.CropX, y + ref.m_info.CropY) - \
                   src.COMPONENT(x + src.m_info.CropX, y + src.m_info.CropY);  \
            dist += (diff * diff);                                             \
        }                                                                      \
    }

    if(ref.isYUV() && src.isYUV())
    {
        if( 0 != id )
        {
            if(    ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400
                || src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
            {
                g_tsStatus.check(MFX_ERR_UNSUPPORTED);
                return 0;
            }
            if(    ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420
                && src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420)
            {
                chroma_step = 2;
                size /= 4;
            }
        }

        switch(id)
        {
        case 0:  GET_DIST(Y, 1); break;
        case 1:  GET_DIST(U, chroma_step); break;
        case 2:  GET_DIST(V, chroma_step); break;
        default: g_tsStatus.check(MFX_ERR_UNSUPPORTED); break;
        }

    } else g_tsStatus.check(MFX_ERR_UNSUPPORTED);

    if (0 == dist)
        return 1000.;

    return (10. * log10(max * max * (size / dist)));
}

mfxF64 SSIM(tsFrame& ref, tsFrame& src, mfxU32 id)
{
    //mfxF64 max  = (1 << 8) - 1;
    mfxF64 C1 =  6.5025;//max * max / 10000;
    mfxF64 C2 = 58.5225;//max * max * 9 / 10000;
    mfxU32 height      = TS_MIN(ref.m_info.CropH, src.m_info.CropH);
    mfxU32 width       = TS_MIN(ref.m_info.CropW, src.m_info.CropW);
    mfxU32 win_width   = 4;
    mfxU32 win_height  = 4;
    mfxU32 win_size    = win_width * win_height;
    mfxU32 win_cnt     = 0;
    mfxU32 chroma_step = 1;
    mfxF64 dist        = 0;

#undef GET_DIST
#define GET_DIST(COMPONENT, STEP)                                                             \
    win_width  *= STEP;                                                                       \
    win_height *= STEP;                                                                       \
    for(mfxU32 j = 0; j <= height - win_height; j += STEP)                                    \
    {                                                                                         \
        for(mfxU32 i = 0; i <= width - win_width; i += STEP)                                  \
        {                                                                                     \
            mfxU32 imeanRef   = 0;                                                            \
            mfxU32 imeanSrc   = 0;                                                            \
            mfxU32 ivarRef    = 0;                                                            \
            mfxU32 ivarSrc    = 0;                                                            \
            mfxU32 icovRefSrc = 0;                                                            \
                                                                                              \
            for(mfxU32 y = j; y < j + win_height; y += STEP)                                  \
            {                                                                                 \
                for(mfxU32 x = i; x < i + win_width; x += STEP)                               \
                {                                                                             \
                    mfxU32 rP = ref.COMPONENT(x + ref.m_info.CropX, y + ref.m_info.CropY);    \
                    mfxU32 sP = src.COMPONENT(x + src.m_info.CropX, y + src.m_info.CropY);    \
                                                                                              \
                    imeanRef   += rP;                                                         \
                    imeanSrc   += sP;                                                         \
                    ivarRef    += rP * rP;                                                    \
                    ivarSrc    += sP * sP;                                                    \
                    icovRefSrc += rP * sP;                                                    \
                }                                                                             \
            }                                                                                 \
                                                                                              \
            mfxF64 meanRef   = (mfxF64) imeanRef / win_size;                                  \
            mfxF64 meanSrc   = (mfxF64) imeanSrc / win_size;                                  \
            mfxF64 varRef    = (mfxF64)(ivarRef    - imeanRef * meanRef) / win_size;          \
            mfxF64 varSrc    = (mfxF64)(ivarSrc    - imeanSrc * meanSrc) / win_size;          \
            mfxF64 covRefSrc = (mfxF64)(icovRefSrc - imeanRef * meanSrc) / win_size;          \
                                                                                              \
            dist += ((meanRef * meanSrc * 2 + C1) * (covRefSrc * 2 + C2)) /                   \
                ((meanRef * meanRef + meanSrc * meanSrc + C1) * (varRef + varSrc + C2));      \
            win_cnt++;                                                                        \
        }                                                                                     \
    }
    
    if(ref.isYUV() && src.isYUV())
    {
        if( 0 != id )
        {
            if(    ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400
                || src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
            {
                g_tsStatus.check(MFX_ERR_UNSUPPORTED);
                return 0;
            }
            if(    ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420
                && src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420)
            {
                chroma_step = 2;
            }
        }

        switch(id)
        {
        case 0:  GET_DIST(Y, 1); break;
        case 1:  GET_DIST(U, chroma_step); break;
        case 2:  GET_DIST(V, chroma_step); break;
        default: g_tsStatus.check(MFX_ERR_UNSUPPORTED); break;
        }

    } else g_tsStatus.check(MFX_ERR_UNSUPPORTED);


    return (dist / win_cnt);
}

bool operator == (const mfxFrameSurface1& s1, const mfxFrameSurface1& s2)
{
    if (!(s1.Info == s2.Info)) return false;
    if (!(s1.Data == s2.Data)) return false;

    return true;
}

bool operator != (const mfxFrameSurface1& s1, const mfxFrameSurface1& s2){return !(s1==s2);}
