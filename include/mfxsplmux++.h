/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.


File Name: mfxsplmux++.h

\* ****************************************************************************** */

#ifndef __MFXSPLMUXPLUSPLUS_H
#define __MFXSPLMUXPLUSPLUS_H

#include "mfxsplmux.h"

class MFXStreamParams {
    mfxStreamParams m_params;
public:
    MFXStreamParams()
        : m_params() {
    }
    const mfxStreamParams* operator & () const {
        return &m_params;
    }
    mfxStreamParams* operator & ()  {
        return &m_params;
    }
    operator const mfxStreamParams& () const {
        return m_params;
    }
    operator mfxStreamParams& () {
        return m_params;
    }
};

struct MFXDataIO
{
    virtual ~MFXDataIO() { }
    virtual mfxI32 Read (mfxBitstream *outBitStream) = 0;
    virtual mfxI32 Write (mfxBitstream *outBitStream) = 0;
    virtual mfxI64 Seek (mfxI64 offset, mfxI32 origin) = 0;
};

class MFXDataIOAdapter
{
    mfxDataIO m_dataIO;
public:
    MFXDataIOAdapter (MFXDataIO& io) {
        m_dataIO.p = &io;
        m_dataIO.Read = staticRead;
        m_dataIO.Seek = staticSeek;
        m_dataIO.Write = staticWrite;
    }
    operator mfxDataIO () const {
        return m_dataIO;
    }
private:
    static mfxI32 staticRead (mfxHDL p, mfxBitstream *outBitStream)
    {
        return reinterpret_cast<MFXDataIO *>(p)->Read(outBitStream);
    }
    static mfxI64 staticSeek (mfxHDL p, mfxI64 offset, mfxI32 origin)
    {
        return reinterpret_cast<MFXDataIO *>(p)->Seek(offset, origin);
    }
    static mfxI32 staticWrite (mfxHDL p, mfxBitstream *outBitStream)
    {
        return reinterpret_cast<MFXDataIO *>(p)->Write(outBitStream);
    }
};

class MFXSplitter
{
public:
    MFXSplitter() : m_spl() { }
    virtual ~MFXSplitter() { Close(); }
    virtual mfxStatus Init(MFXDataIO &dataIO) {
        return MFXSplitter_Init(MFXDataIOAdapter(dataIO), &m_spl);
    }
    virtual mfxStatus Close() { return MFXSplitter_Close(m_spl); }
    virtual mfxStatus GetInfo(MFXStreamParams &par) { return MFXSplitter_GetInfo(m_spl, &par); }
    virtual mfxStatus GetBitstream(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXSplitter_GetBitstream(m_spl, iTrack, Bitstream); }
    virtual mfxStatus ReleaseBitstream(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXSplitter_ReleaseBitstream(m_spl, iTrack, Bitstream); }

protected:
    mfxSplitter m_spl;
};

class MFXMuxer
{
public:
    MFXMuxer() : m_mux() { }
    virtual ~MFXMuxer() { Close(); }
    virtual mfxStatus Init(MFXStreamParams &par, MFXDataIO &dataIO){
        return MFXMuxer_Init(&par, MFXDataIOAdapter(dataIO), &m_mux);
    }
    virtual mfxStatus Close() { return MFXMuxer_Close(m_mux); }
    virtual mfxStatus PutData(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXMuxer_PutData(m_mux, iTrack, Bitstream); }

protected:
    mfxMuxer m_mux;
};

#endif // __MFXSPLMUXPLUSPLUS_H