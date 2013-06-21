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

class MFXSplitterParam {
    mfxSplitterParam m_param;

public:
    MFXSplitterParam(mfxStreamInfo* StreamInfo = NULL, mfxU32 Flags = 0)
        : m_param() {
        m_param.StreamInfo = StreamInfo;
        m_param.Flags = Flags;
    }
    operator const mfxSplitterParam& () const {
        return m_param;
    }
    operator mfxSplitterParam& () {
        return m_param;
    }
};

class MFXMuxerParam {
    mfxMuxerParam m_param;

public:
    MFXMuxerParam(mfxStreamInfo* StreamInfo = NULL)
        : m_param() {
        m_param.StreamInfo = StreamInfo;
    }
    operator const mfxMuxerParam& () const {
        return m_param;
    }
    operator mfxMuxerParam& () {
        return m_param;
    }
};

class MFXDataIO
{
public:
    virtual ~MFXDataIO() { }
    virtual mfxI32 Read (mfxBitstream *outBitStream) = 0;
    virtual mfxI32 Write (mfxBitstream *outBitStream) = 0;
    virtual mfxI64 Seek (mfxI64 offset, mfxI32 origin) = 0;
};

class MFXSplitter
{
    static mfxI32 staticRead (mfxHDL p, mfxBitstream *outBitStream)
    {
        return reinterpret_cast<MFXSplitter*>(p)->_dataIO->Read(outBitStream);
    }
    static mfxI64 staticSeek (mfxHDL p, mfxI64 offset, mfxI32 origin)
    {
        return reinterpret_cast<MFXSplitter*>(p)->_dataIO->Seek(offset, origin);
    }
    MFXDataIO *_dataIO;
    mfxDataIO io;
public:
    MFXSplitter() : m_spl() { }
    virtual ~MFXSplitter() { Close(); }

    mfxStatus Init(MFXSplitterParam &par, MFXDataIO &dataIO)
    {
        _dataIO = &dataIO;
        io.p = this;
        io.Read = staticRead;
        io.Seek = staticSeek;
        return MFXSplitter_Init(&(mfxSplitterParam)par, io, &m_spl);
    }
    mfxStatus Close() { return MFXSplitter_Close(m_spl); }
    mfxStatus GetInfo(MFXSplitterParam &par) { return MFXSplitter_GetInfo(m_spl, &(mfxSplitterParam)par); }
    mfxStatus GetBitstream(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXSplitter_GetBitstream(m_spl, iTrack, Bitstream); }
    mfxStatus ReleaseBitstream(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXSplitter_ReleaseBitstream(m_spl, iTrack, Bitstream); }

protected:
    mfxSplitter m_spl;
};

class MFXMuxer
{
    static mfxI32 staticWrite (mfxHDL p, mfxBitstream *outBitStream)
    {
        return reinterpret_cast<MFXMuxer*>(p)->_dataIO->Write(outBitStream);
    }
    MFXDataIO *_dataIO;
    mfxDataIO io;
public:
    MFXMuxer() : m_mux() { }
    virtual ~MFXMuxer() { Close(); }

    mfxStatus Init(MFXMuxerParam &par, MFXDataIO &dataIO)
    {
        _dataIO = &dataIO;
        io.p = this;
        io.Write = staticWrite;
        return MFXMuxer_Init(&(mfxMuxerParam)par, io, &m_mux);
    }
    mfxStatus Close() { return MFXMuxer_Close(m_mux); }
    mfxStatus PutData(mfxU32 iTrack, mfxBitstream *Bitstream) { return MFXMuxer_PutData(m_mux, iTrack, Bitstream); }

protected:
    mfxMuxer m_mux;
};

#endif // __MFXSPLMUXPLUSPLUS_H
