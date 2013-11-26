/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once
#include <limits>
#include "mfx_ibitstream_reader.h"

//read not whole buffer, but by exact data portions
class ExactLenghtBsReader
    : public InterfaceProxy<IBitstreamReader>
{
    typedef InterfaceProxy<IBitstreamReader> base;
    size_t m_nBytesRead;
    size_t m_nChunkSize;
public:
    ExactLenghtBsReader(size_t len, IBitstreamReader *pTarget)
        : base(pTarget)
        , m_nBytesRead()
        , m_nChunkSize(len)
    {
    }
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        if (m_nBytesRead == m_nChunkSize)
        {
            //reset num bytes read
            m_nBytesRead = 0;
        }

        size_t nAvail = BSUtil::Reserve(&bs, m_nChunkSize - m_nBytesRead);
        size_t nBytesToRead = (std::min)(m_nChunkSize - m_nBytesRead, nAvail);
        size_t nOriginalBufferSize = bs.MaxLength;

        //setbuffersize to exact number of bytes to be read
        bs.MaxLength = (mfxU32)(bs.DataOffset + bs.DataLength + nBytesToRead);

        mfxStatus sts = base::ReadNextFrame(bs);

        if (MFX_ERR_NONE == sts)
        {
            m_nBytesRead += nBytesToRead;
        }

        bs.MaxLength = (mfxU32)nOriginalBufferSize;

        return sts;
    }
};
