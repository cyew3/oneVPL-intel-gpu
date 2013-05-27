/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_ibitstream_reader.h"
#include "mfx_imultiple.h"
#include <functional>

class MultiReader
    : public IMultiple<IBitstreamReader>
{
public:
    MultiReader()
    {
        RegisterFunction(&MultiReader::ReadNextFrame);
    }
    virtual void Close()
    {
        ForEach(std::mem_fun(&IBitstreamReader::Close));
    }
    virtual mfxStatus Init(const vm_char * /*strFileName*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        for (size_t n = 0; n < m_items.size(); n++)
        {
            bs.DependencyId = (mfxU16)CurrentItemIdx(&MultiReader::ReadNextFrame);
            IBitstreamReader *current = NextItemForFunction(&MultiReader::ReadNextFrame);
            mfxStatus  sts = current->ReadNextFrame(bs);
            if (sts != MFX_ERR_UNKNOWN)
                return sts;
        }
        //eos on all input streams
        return MFX_ERR_UNKNOWN;
    }
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams)
    {
        IBitstreamReader *first = ItemFromIdx(0);
        return first->GetStreamInfo(pParams);
    }
    virtual mfxStatus SeekTime(mfxF64 fSeekTo)
    {
        ForEach(std::bind2nd(std::mem_fun(&IBitstreamReader::SeekTime), fSeekTo));
        //todo:fix
        return MFX_ERR_NONE;
    }
    virtual mfxStatus SeekPercent(mfxF64 fSeekTo)
    {
        ForEach(std::bind2nd(std::mem_fun(&IBitstreamReader::SeekPercent), fSeekTo));
        //todo:fix
        return MFX_ERR_NONE;
    }
    virtual mfxStatus SeekFrameOffset(mfxU32 /*nFrameOffset*/, mfxFrameInfo &/*in_info*/)
    {
        return MFX_ERR_NONE;
    }
 };