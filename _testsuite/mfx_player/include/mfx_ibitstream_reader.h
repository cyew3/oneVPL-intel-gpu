/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfxstructures.h"
#include "mfx_bitstream2.h"
#include "vm_strings.h"
#include "mfx_iproxy.h"
#include "mfx_io_utils.h"

struct sStreamInfo
{
    mfxU32 videoType;
    mfxU16 CodecProfile;
    mfxU16 nWidth;
    mfxU16 nHeight;
    bool   isDefaultFC;
};

//file reader abstraction
class IBitstreamReader : EnableProxyForThis<IBitstreamReader>
{
public:
    virtual ~IBitstreamReader(){}
    virtual void      Close() = 0;
    virtual mfxStatus Init(const vm_char *strFileName) = 0;
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs) = 0;
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams) = 0;
    virtual mfxStatus SeekTime(mfxF64 fSeekTo) = 0;
    virtual mfxStatus SeekPercent(mfxF64 fSeekTo) = 0;
    //reposition to specific frames number offset in  input stream
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info) = 0;
};

template <>
class InterfaceProxy<IBitstreamReader>
    : public InterfaceProxyBase<IBitstreamReader>
{
public:
    InterfaceProxy(IBitstreamReader * pTarget)
        : InterfaceProxyBase<IBitstreamReader>(pTarget)
    {
    }
    virtual void      Close()
    {
        m_pTarget->Close();
    }
    virtual mfxStatus Init(const vm_char *strFileName)
    {
        return m_pTarget->Init(strFileName);
    }
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        return m_pTarget->ReadNextFrame(bs);
    }
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams)
    {
        return m_pTarget->GetStreamInfo(pParams);
    }
    virtual mfxStatus SeekTime(mfxF64 fSeekTo)
    {
        return m_pTarget->SeekTime(fSeekTo);
    }
    virtual mfxStatus SeekPercent(mfxF64 fSeekTo) 
    {
        return m_pTarget->SeekPercent(fSeekTo);
    }
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info)
    {
        return m_pTarget->SeekFrameOffset(nFrameOffset, in_info);
    }
};