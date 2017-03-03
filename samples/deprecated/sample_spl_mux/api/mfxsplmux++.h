/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __MFXSPLMUXPLUSPLUS_H
#define __MFXSPLMUXPLUSPLUS_H

#include <stdlib.h>
#include <cstring>

#include "mfxsplmux.h"

class MFXStreamParams : public mfxStreamParams {
public:
    MFXStreamParams() {
        SystemType = MFX_UNDEF_STREAM;
        Duration = 0;
        NumTracks = 0;
        NumTracksAllocated = 0;
        TrackInfo = 0;
        Flags = 0;
    }
};

class MFXDataIO
{
public:
    virtual ~MFXDataIO() { }
    virtual mfxI32 Read (mfxBitstream *bs) = 0;
    virtual mfxI32 Write (mfxBitstream *bs) = 0;
    virtual mfxI64 Seek (mfxI64 offset, mfxSeekOrigin origin) = 0;
};

class MFXDataIOAdapter
{
    mfxDataIO m_data_io;
public:
    MFXDataIOAdapter (MFXDataIO& data_io) {
        m_data_io.pthis = &data_io;
        m_data_io.Read = staticRead;
        m_data_io.Seek = staticSeek;
        m_data_io.Write = staticWrite;
    }
    operator mfxDataIO () const {
        return m_data_io;
    }
private:
    static mfxI32 staticRead (mfxHDL pthis, mfxBitstream *bs)
    {
        return reinterpret_cast<MFXDataIO *>(pthis)->Read(bs);
    }
    static mfxI64 staticSeek (mfxHDL pthis, mfxI64 offset, mfxSeekOrigin origin)
    {
        return reinterpret_cast<MFXDataIO *>(pthis)->Seek(offset, origin);
    }
    static mfxI32 staticWrite (mfxHDL pthis, mfxBitstream *bs)
    {
        return reinterpret_cast<MFXDataIO *>(pthis)->Write(bs);
    }
};

class MFXSplitter
{
public:
    MFXSplitter() : m_spl(), io(), m_trackInfo(NULL), m_tracksNum(0) { }
    virtual ~MFXSplitter()
    {
        freeTrackInfo();
        Close();
    }
    virtual mfxStatus Init(MFXDataIO &data_io)
    {
        io = MFXDataIOAdapter(data_io) ;
        return MFXSplitter_Init(&io, &m_spl);
    }
    virtual mfxStatus Close() {
        mfxStatus sts = MFXSplitter_Close(m_spl);
        m_spl = (mfxSplitter)0;
        return sts;
    }
    virtual mfxStatus GetInfo(MFXStreamParams &par)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxStreamParams *splitterPar = &par;
        sts = MFXSplitter_GetInfo(m_spl, splitterPar);
        if (sts == MFX_ERR_NONE)
        {
            freeTrackInfo();
            m_trackInfo = (mfxTrackInfo**) malloc(sizeof(mfxTrackInfo*)*splitterPar->NumTracks);
            if (!m_trackInfo)
                sts = MFX_ERR_MEMORY_ALLOC;
            m_tracksNum = splitterPar->NumTracks;
        }
        if (sts == MFX_ERR_NONE)
        {
            memset(m_trackInfo, 0, sizeof(mfxTrackInfo*)*splitterPar->NumTracks);
            for (mfxU32 i = 0; i < splitterPar->NumTracks && sts == MFX_ERR_NONE; i++)
            {
                m_trackInfo[i] = (mfxTrackInfo*) malloc(sizeof(mfxTrackInfo));
                if (!m_trackInfo[i])
                    sts = MFX_ERR_MEMORY_ALLOC;
            }
        }
        if (sts == MFX_ERR_NONE)
        {
            splitterPar->NumTracksAllocated = splitterPar->NumTracks;
            splitterPar->TrackInfo = m_trackInfo;
            sts = MFXSplitter_GetInfo(m_spl, splitterPar);
        }
        if (sts != MFX_ERR_NONE)
        {
            freeTrackInfo();
        }
        return sts;
    }
    virtual mfxStatus GetBitstream(mfxU32 *track_num, mfxBitstream *bs) { return MFXSplitter_GetBitstream(m_spl, track_num, bs); }
    virtual mfxStatus ReleaseBitstream(mfxBitstream *bs) { return MFXSplitter_ReleaseBitstream(m_spl, bs); }
    virtual mfxStatus Seek(mfxU64 timestamp) { return MFXSplitter_Seek(m_spl, timestamp); }

protected:
    void freeTrackInfo()
    {
        if (m_trackInfo)
        {
            for (int i = 0; i < m_tracksNum; i++)
            {
                if (m_trackInfo[i])
                {
                    free (m_trackInfo[i]);
                    m_trackInfo[i] = NULL;
                }
            }
            free(m_trackInfo);
            m_trackInfo = NULL;
        }
    }

    mfxSplitter m_spl;
    mfxDataIO   io;
    mfxTrackInfo **m_trackInfo;
    mfxU16      m_tracksNum;
};

class MFXMuxer
{
public:
    MFXMuxer() : m_mux(), io() { }
    virtual ~MFXMuxer() { Close(); }
    virtual mfxStatus Init(MFXStreamParams &par, MFXDataIO &data_io) {
        io = MFXDataIOAdapter(data_io) ;
        return MFXMuxer_Init(&par, &io, &m_mux);
    }
    virtual mfxStatus Close() {
        mfxStatus sts = MFXMuxer_Close(m_mux);
        m_mux = (mfxMuxer)0;
        return sts;
    }
    virtual mfxStatus PutBitstream(mfxU32 track_num, mfxBitstream *bs, mfxU64 duration) { return MFXMuxer_PutBitstream(m_mux, track_num, bs, duration); }

protected:
    mfxMuxer  m_mux;
    mfxDataIO io;
};

#endif // __MFXSPLMUXPLUSPLUS_H