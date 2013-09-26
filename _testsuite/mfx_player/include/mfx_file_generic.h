/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"

class GenericFile
    : public IFile
{
    IMPLEMENT_CLONE(GenericFile);

public:

    GenericFile()
        : m_pFD()
    {
    }

    ~GenericFile()
    {
        Close();
    }

    mfxStatus GetLastError()
    {
#ifdef WIN32
        int r = ::GetLastError();
        //kernel pool is out of memory
        if (ERROR_NO_SYSTEM_RESOURCES == r)
        {
            return MFX_WRN_DEVICE_BUSY;
        }

#endif
        //on Linux we wont wait since dont have getlast error fnc
        return MFX_ERR_NONE;
    }

    mfxStatus GetInfo(const tstring & path, mfxU64 &attr)
    {
        MFX_CHECK_WITH_ERR( 0 != vm_file_getinfo(path.c_str(), &attr, NULL), MFX_ERR_UNKNOWN);
        return MFX_ERR_NONE;
    }

    mfxStatus Open(const tstring & path, const tstring& access)
    {
        Close();

        MFX_CHECK_VM_FOPEN(m_pFD, path.c_str(), access.c_str());
        
        return MFX_ERR_NONE;
    }
    
    mfxStatus Reopen()
    {
        return MFX_ERR_NONE;
    }

    mfxStatus Close()
    {
        if (NULL != m_pFD)
        {
            vm_file_close(m_pFD);
            m_pFD = NULL;
        }
        return MFX_ERR_NONE;
    }
    mfxStatus Read(mfxU8* p, mfxU32 &size)
    {
        MFX_CHECK_POINTER(m_pFD);
        
        size = (mfxU32)vm_file_fread(p, 1, size, m_pFD);
        
        return MFX_ERR_NONE;
    }
    mfxStatus ReadLine(vm_char* p, const mfxU32 &size)
    {
        MFX_CHECK_POINTER(m_pFD);

        vm_char *pChar = vm_file_fgets(p, size, m_pFD);
        
        return MFX_ERR_NONE;
    }
    mfxStatus Write(mfxU8* p, mfxU32 size)
    {
        if (NULL == m_pFD)
            return MFX_ERR_NONE;
        
        MFX_CHECK(size == (mfxU32)vm_file_fwrite(p, 1, size, m_pFD));

        return MFX_ERR_NONE;    
    }
    mfxStatus Write(mfxBitstream * pBs)
    {
        if (NULL == pBs)
            return MFX_ERR_NONE;
        
        return Write(pBs->Data + pBs->DataOffset, pBs->DataLength);
    }
    mfxStatus Seek(Ipp64s position, VM_FILE_SEEK_MODE mode)
    {
        if (NULL == m_pFD)
            return MFX_ERR_NONE;

        MFX_CHECK(0 == vm_file_fseek(m_pFD, position, mode));

        return MFX_ERR_NONE;
    }
    bool isOpen()
    {
        return NULL != m_pFD;
    }
    bool isEOF()
    {
        if (NULL == m_pFD)
            return true;
        return !(0 == vm_file_feof(m_pFD));
    }


protected:

    GenericFile(const GenericFile & /*that*/)
        : m_pFD()
    {
    }

    vm_file *m_pFD;
};
