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

    bool Read(mfxU8* dst, mfxU32 size);
};

class tsBitstreamProcessor
{
public:
    virtual mfxStatus ProcessBitstream(mfxBitstream& bs){ return MFX_ERR_NONE; };
};

class tsBitstreamErazer : public tsBitstreamProcessor
{
public:
    tsBitstreamErazer() {};
    ~tsBitstreamErazer() {};
    mfxStatus ProcessBitstream(mfxBitstream& bs){ bs.DataLength = 0; return MFX_ERR_NONE; }
};

class tsBitstreamWriter : public tsBitstreamProcessor
{
private:
    FILE* m_file;
public:
    tsBitstreamWriter(const char* fname);
    ~tsBitstreamWriter();
    mfxStatus ProcessBitstream(mfxBitstream& bs);
};