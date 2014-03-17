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

bool tsReader::Read(mfxU8* dst, mfxU32 size)
{
    if(m_file)
    {
        return (size == (mfxU32)fread(dst, 1, size, m_file));
    }
    if (DataLength >= size)
    {
        memcpy(dst, Data + DataOffset, size);
        DataOffset += size;
        DataLength -= size;
        return true;
    }

    return false;
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