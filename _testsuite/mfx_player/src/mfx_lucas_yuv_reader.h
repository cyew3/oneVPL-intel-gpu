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

#include "lucas.h"

//turnoff whole class in case of missing lucas configuration, to avoid include under defines
#ifdef LUCAS_DLL
#include "mfx_io_utils.h"

class LucasYUVReader : public CYUVReader
{
public :
    LucasYUVReader()
        : m_lucas(&lucas::CLucasCtx::Instance())
    {
    }
    ~LucasYUVReader()
    {
        Close();
    }
    void    Close()
    {
        if (!(*m_lucas)->read)
        {
            CYUVReader::Close();
        }
    }
    mfxStatus  Init(const vm_char *strFileName)
    {
        if(!(*m_lucas)->read) {
            Close();
            return CYUVReader::Init(strFileName);
        }
        return MFX_ERR_NONE;
    }

protected:
    mfxU32 RawRead(mfxU8 *buff, mfxU32 count)
    {
        return ((*m_lucas)->read ? (*m_lucas)->read((*m_lucas)->fmWrapperPtr, (char *)buff, count) : CYUVReader::RawRead(buff, count)); 
    }
    lucas::CLucasCtx *m_lucas;
};

#endif