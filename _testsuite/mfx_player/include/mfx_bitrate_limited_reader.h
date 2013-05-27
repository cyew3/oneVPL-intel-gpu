/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_itime.h"
#include "mfx_ibitstream_reader.h"

class BitrateLimitedReader 
    : public InterfaceProxy<IBitstreamReader>
{
    typedef InterfaceProxy<IBitstreamReader> base;
public:

    BitrateLimitedReader(mfxU32 nBytesSec
        , ITime *pTime
        , mfxU32 nMinChunkSize //consumer will be fed with data of this chunk size emulating network UDP packets
        , IBitstreamReader *);

    mfxStatus ReadNextFrame(mfxBitstream2 &pBS);


protected:

    mfxStatus PullData();
    void ControlBitrate();

    mfxBitstream2      m_inputBs;
    mfxU32             m_nBytesPerSec;
    mfxU32             m_nMinChunkSize;
    ITime            * m_pTime;
    mfxU64             m_nBytesWritten;
    bool               m_bEOS;
    mfxF64             m_firstDataWrittenTick;
    std::vector <mfxU8>m_bsData;
};