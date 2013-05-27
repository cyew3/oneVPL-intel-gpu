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

#include "lucas.h"

#ifdef LUCAS_DLL

#include "mfx_bitstream_reader.h"

class LucasBsReader
    : public BitstreamReader
{
    LucasBsReader(sStreamInfo * pParams = NULL)
        : m_lucas(&lucas::CLucasCtx::Instance())
        , BitstreamReader(pParams)

    {
    }
    virtual void Close()
    {
        if ((*m_lucas)->read)
        {
            BitstreamReader::Close();
        }
    }
    virtual mfxStatus Init(const vm_char *strFileName)
    {
        Close();

        if(!(*m_lucas)->read) {
            return BitstreamReader::Init(strFileName);
        }
        return MFX_ERR_NONE;
    }
protected:
    mfxU32 RawRead(mfxU8 *buff, mfxU32 count)
    {
        return ((*m_lucas)->read ? (*m_lucas)->read((*m_lucas)->fmWrapperPtr, (char *)buff, count) : BitstreamReader::RawRead(buff, count)) 
    }

    lucas::CLucasCtx *m_lucas;

    vm_file*    m_fSource;
    bool        m_bInited;
    sStreamInfo m_sInfo;
    bool        m_bInfoSet;
    CBitstreamReader m_CbsReader;
};

#endif