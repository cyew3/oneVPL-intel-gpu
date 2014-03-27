#pragma once

#include "ts_common.h"

class tsReader : mfxBitstream
{
private:
    FILE* m_file;
public:
    tsReader(mfxBitstream bs);
    tsReader(const char* fname);
    ~tsReader();

    mfxU32 Read(mfxU8* dst, mfxU32 size);
};

class tsBitstreamProcessor
{
public:
    mfxBitstream* ProcessBitstream(mfxBitstream& bs);
    virtual mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames){ return MFX_ERR_NONE; };
};

class tsBitstreamErazer : public tsBitstreamProcessor
{
public:
    tsBitstreamErazer() {};
    ~tsBitstreamErazer() {};
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames){ bs.DataLength = 0; return MFX_ERR_NONE; }
};

class tsBitstreamWriter : public tsBitstreamProcessor
{
private:
    FILE* m_file;
public:
    tsBitstreamWriter(const char* fname);
    ~tsBitstreamWriter();
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

class tsBitstreamReader : public tsBitstreamProcessor, public tsReader
{
public:
    bool    m_eos;
    mfxU8*  m_buf;
    mfxU32  m_buf_size;

    tsBitstreamReader(const char* fname, mfxU32 buf_size);
    tsBitstreamReader(mfxBitstream bs, mfxU32 buf_size);
    ~tsBitstreamReader();
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

class tsBitstreamReaderIVF : public tsBitstreamReader
{
public:
    bool m_first_call;
    tsBitstreamReaderIVF(const char* fname, mfxU32 buf_size) : tsBitstreamReader(fname, buf_size), m_first_call(true) {};
    tsBitstreamReaderIVF(mfxBitstream bs, mfxU32 buf_size) : tsBitstreamReader(bs, buf_size), m_first_call(true)  {};
    ~tsBitstreamReaderIVF(){};
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};