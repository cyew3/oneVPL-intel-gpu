//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

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
