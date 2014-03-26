#include "ts_bitstream.h"

tsReader::tsReader(mfxBitstream bs)
    : mfxBitstream(bs)
    , m_file(0)
{
}

tsReader::tsReader(const char* fname)
    : mfxBitstream()
    , m_file(0)
{
    
#pragma warning(disable:4996)
    m_file = fopen(fname, "rb");    
#pragma warning(default:4996)
}

tsReader::~tsReader()
{
    if(m_file)
    {
        fclose(m_file);
    }
}

mfxU32 tsReader::Read(mfxU8* dst, mfxU32 size)
{
    if(m_file)
    {
        return (mfxU32)fread(dst, 1, size, m_file);
    }
    else 
    {
        mfxU32 sz = TS_MIN(DataLength, size);

        memcpy(dst, Data + DataOffset, sz);

        DataOffset += sz;
        DataLength -= sz;

        return sz;
    }
}

mfxBitstream* tsBitstreamProcessor::ProcessBitstream(mfxBitstream& bs)
{
    if(ProcessBitstream(bs, 1))
    {
        return 0;
    }

    return &bs;
}


tsBitstreamWriter::tsBitstreamWriter(const char* fname)
{
    
#pragma warning(disable:4996)
    m_file = fopen(fname, "wb");  
#pragma warning(default:4996)
}

tsBitstreamWriter::~tsBitstreamWriter()
{
    if(m_file)
    {
        fclose(m_file);
    }
}
    
mfxStatus tsBitstreamWriter::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    if(bs.DataLength != fwrite(bs.Data + bs.DataOffset, 1, bs.DataLength, m_file))
    {
        return MFX_ERR_UNKNOWN;
    }
    bs.DataLength = 0;
    return MFX_ERR_NONE; 
}

tsBitstreamReader::tsBitstreamReader(const char* fname, mfxU32 buf_size)
    : tsReader(fname)
    , m_eos(false)
    , m_buf(new mfxU8[buf_size])
    , m_buf_size(buf_size)
{
}

tsBitstreamReader::~tsBitstreamReader()
{
    if(m_buf)
    {
        delete[] m_buf;
    }
}

mfxStatus tsBitstreamReader::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    if(m_eos)
    {
        return MFX_ERR_MORE_DATA;
    }

    if(bs.DataLength + bs.DataOffset > m_buf_size)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    bs.Data      = m_buf;
    bs.MaxLength = m_buf_size;

    if(bs.DataLength && bs.DataOffset)
    {
        memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
    }
    bs.DataOffset = 0;

    bs.DataLength += Read(bs.Data + bs.DataLength, bs.MaxLength - bs.DataLength);

    m_eos = bs.DataLength != bs.MaxLength;

    return MFX_ERR_NONE;
}

