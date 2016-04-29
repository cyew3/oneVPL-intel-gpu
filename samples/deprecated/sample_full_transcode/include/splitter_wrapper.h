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

#pragma once

#include <memory>
#include "isample.h"
#include "mfxsplmux++.h"
#include "fileio.h"
#include "exceptions.h"

//returned by splitterwrapepr::getsample
class SplitterSample : public SampleBitstream1, private no_copy {
    MFXSplitter& m_spl;
    mfxBitstream m_bs;
    //TODO : splitter release bitstream WA
    mfxBitstream m_bsToRelease;
public:
    SplitterSample(MFXSplitter& splitter, int TrackId, const mfxBitstream& bits) :
        SampleBitstream1(m_bs, TrackId),
        m_spl(splitter),
        m_bs(bits) ,
        m_bsToRelease(bits) {
    }

    virtual ~SplitterSample() {
        m_spl.ReleaseBitstream(&m_bsToRelease);
    }
};

class ISplitterWrapper {
public:
    virtual ~ISplitterWrapper() {}
    virtual bool GetSample(SamplePtr & sample) = 0;
    virtual void GetInfo(MFXStreamParams & info) = 0;
};

class SplitterWrapper : public ISplitterWrapper, private no_copy
{
protected:
    std::auto_ptr<MFXSplitter> m_splitter;
    std::auto_ptr<MFXDataIO> m_io;
    bool m_EOF;
    int  n_NullIds;
    int  n_TracksInFile;
public:
    SplitterWrapper( std::auto_ptr<MFXSplitter>& splitter
                    , std::auto_ptr<MFXDataIO>&io)
                   : m_splitter(splitter)
                   , m_io (io)
                   , m_EOF(false)
                   , n_NullIds(0)
                   , n_TracksInFile(0) {
        InitSplitter();
        MFXStreamParams info;
        GetInfo(info);
        n_TracksInFile = info.NumTracks;
    }
    virtual ~SplitterWrapper() {
        m_splitter->Close();
    }

    virtual bool GetSample(SamplePtr & sample);
    virtual void GetInfo(MFXStreamParams & info);

protected:
    void InitSplitter();
};
