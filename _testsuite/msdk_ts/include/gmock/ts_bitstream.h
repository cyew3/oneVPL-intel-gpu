/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "ts_common.h"
#include <ipp.h>

class tsReader : public mfxBitstream
{
private:
    FILE* m_file;
public:
    tsReader(mfxBitstream bs);
    tsReader(const char* fname);
    virtual ~tsReader();

    mfxU32 Read(mfxU8* dst, mfxU32 size);
    mfxStatus SeekToStart();
};

class tsBitstreamProcessor
{
public:
    virtual ~tsBitstreamProcessor() { }

    mfxBitstream* ProcessBitstream(mfxBitstream& bs);
    virtual mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames){ return MFX_ERR_NONE; };
};

class tsBitstreamErazer : public tsBitstreamProcessor
{
public:
    tsBitstreamErazer() {};
    virtual ~tsBitstreamErazer() {};
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames){ bs.DataLength = 0; return MFX_ERR_NONE; }
};

class tsBitstreamWriter : public tsBitstreamProcessor
{
private:
    FILE* m_file;
public:
    tsBitstreamWriter(const char* fname, bool append = false);
    virtual ~tsBitstreamWriter();
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

inline mfxStatus WriteBitstream(const char* fname, mfxBitstream& bs, bool append = false)
{
    tsBitstreamWriter wr(fname, append);
    return wr.ProcessBitstream(bs, 1);
}

class tsBitstreamReader : public tsBitstreamProcessor, public tsReader
{
public:
    bool    m_eos;
    mfxU8*  m_buf;
    mfxU32  m_buf_size;

    tsBitstreamReader(const char* fname, mfxU32 buf_size);
    tsBitstreamReader(mfxBitstream bs, mfxU32 buf_size);
    virtual ~tsBitstreamReader();
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

class tsBitstreamReaderIVF : public tsBitstreamReader
{
public:
    bool m_first_call;
    tsBitstreamReaderIVF(const char* fname, mfxU32 buf_size) : tsBitstreamReader(fname, buf_size), m_first_call(true) {};
    tsBitstreamReaderIVF(mfxBitstream bs, mfxU32 buf_size) : tsBitstreamReader(bs, buf_size), m_first_call(true)  {};
    virtual ~tsBitstreamReaderIVF(){};
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

class tsSplitterHEVCES : public tsBitstreamReader
{
public:
    mfxBitstream m_intBS; //internal bitstream buffer (not AU-aligned)

    tsSplitterHEVCES(const char* fname, mfxU32 buf_size)
        : tsBitstreamReader(fname, buf_size)
    {
        memset(&m_intBS, 0, sizeof(m_intBS));
    }

    tsSplitterHEVCES(mfxBitstream bs, mfxU32 buf_size)
        : tsBitstreamReader(bs, buf_size)
    {
        memset(&m_intBS, 0, sizeof(m_intBS));
    }

    virtual ~tsSplitterHEVCES(){}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

class tsBitstreamCRC32 : public tsBitstreamProcessor
{
    Ipp32u m_crc;
public:
    bool    m_eos;
    mfxU8*  m_buf;
    mfxU32  m_buf_size;
    const char* m_dump;

    inline Ipp32u GetCRC() { return m_crc; }
    tsBitstreamCRC32()
        : m_crc(0)
        , m_eos(false)
        , m_buf(new mfxU8[1024*1024])
        , m_buf_size(1024*1024)
        , m_dump(0)
    {
    }
    tsBitstreamCRC32(mfxBitstream bs, mfxU32 buf_size);
    virtual ~tsBitstreamCRC32();
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};
