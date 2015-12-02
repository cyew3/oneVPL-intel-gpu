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
    EXPECT_NE((void*) 0, m_file) << "ERROR: cannot open file `" << fname << "'\n";
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

mfxStatus tsReader::SeekToStart()
{
    fseek(m_file, 0, SEEK_SET);
    return MFX_ERR_NONE;
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

tsBitstreamReader::tsBitstreamReader(mfxBitstream bs, mfxU32 buf_size)
    : tsReader(bs)
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
    if(m_eos && bs.DataLength < 4)
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


mfxStatus tsBitstreamReaderIVF::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 header_size = m_first_call * 32 + 12;
    mfxU32 frame_size = 0;

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

    if(header_size != Read(bs.Data + bs.DataLength, header_size))
    {
        m_eos = true;
        return MFX_ERR_MORE_DATA;
    }

    if(m_first_call)
    {
        bs.DataLength += header_size; //assumed 1st call for DecodeHeader
        m_first_call = false;
    } 
    else 
    {
        bs.DataOffset += header_size;
    }

    frame_size = *(mfxU32*)(bs.Data + bs.DataOffset + bs.DataLength - 12);

    if(bs.MaxLength - bs.DataLength < frame_size)
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if(frame_size != Read(bs.Data + bs.DataOffset + bs.DataLength, frame_size))
    {
        m_eos = true;
        return MFX_ERR_MORE_DATA;
    }
    bs.DataLength += frame_size;
    bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    

    return MFX_ERR_NONE;
}


tsBitstreamCRC32::tsBitstreamCRC32(mfxBitstream bs, mfxU32 buf_size)
    : m_eos(false)
    , m_buf(new mfxU8[buf_size])
    , m_buf_size(buf_size)
    , m_crc(0)
{
}

tsBitstreamCRC32::~tsBitstreamCRC32()
{
    if(m_buf)
    {
        delete[] m_buf;
    }
}

mfxStatus tsBitstreamCRC32::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    if (!bs.DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    IppStatus sts = ippsCRC32_8u(bs.Data, bs.DataLength, &m_crc);
    if (sts != ippStsNoErr)
    {
        g_tsLog << "ERROR: cannot calculate CRC32 IppStatus=" << sts << "\n";
        return MFX_ERR_ABORTED;
    }

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}
