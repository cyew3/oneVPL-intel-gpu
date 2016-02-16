/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"
#include "mfx_pipeline_utils.h"
#include "ippdc.h"
#include "ippcp.h"
#include "vm_file.h"

//wrapping to file writer render provided crc calc+file writing
class CRCFileWriter
    : public InterfaceProxy<IFile>
{
    typedef InterfaceProxy<IFile> base;
    IMPLEMENT_CLONE(CRCFileWriter);
    mfxU32  m_crc32;
    tstring m_crcFile;
public:
    CRCFileWriter(const tstring &sCrcFile, std::auto_ptr<IFile>& pTargetFile)
        : base(pTargetFile)
        , m_crc32()
        , m_crcFile(sCrcFile)
    {
        int size;
        ippsMD5GetSize(&size);
        ctx = (IppsMD5State *)malloc(size);
        if ( ctx )
            ippsMD5Init(ctx);
    }
    ~CRCFileWriter()
    {
        if (ctx)
            ippsMD5Final(md5_digest, ctx);

        vm_char md5_result[33] = {0};
        for(int i = 0; i < 16; i++)
        {
            vm_string_sprintf(&md5_result[i*2], VM_STRING("%02x"), md5_digest[i]);
        }
        PrintInfo(VM_STRING("MD5"), md5_result);

        PrintInfo(VM_STRING("CRC"), VM_STRING("%08X"), m_crc32 );
        WriteCrcToFile();

        if (ctx)
            free(ctx);
    }
    virtual mfxStatus Write(mfxU8* p, mfxU32 size)
    {
        if (NULL != p)
        {
            ippsCRC32_8u(p, size, &m_crc32);
            if (ctx) ippsMD5Update(p, size, ctx);
        }
        return base::Write(p, size);
    }
    virtual mfxStatus Write(mfxBitstream * pBs)
    {
        if (NULL != pBs)
        {
            ippsCRC32_8u(pBs->DataOffset + pBs->Data, pBs->DataLength, &m_crc32);
            if (ctx) ippsMD5Update(pBs->DataOffset + pBs->Data, pBs->DataLength, ctx);
        }
        return base::Write(pBs);
    }

protected:
    CRCFileWriter(const CRCFileWriter &that)
        : base((const base&)that)
        , m_crc32()
    {
        int size;
        ippsMD5GetSize(&size);
        ctx = (IppsMD5State *)malloc(size);
        if ( ctx )
            ippsMD5Init(ctx);
    }
    mfxStatus WriteCrcToFile()
    {
        if (!m_crcFile.empty())
        {
            vm_file *f ;
            MFX_CHECK_VM_FOPEN(f, m_crcFile.c_str(), VM_STRING("wb"));
        
            if (!vm_file_write(&m_crc32, 1, sizeof(m_crc32), f))
                return MFX_ERR_UNKNOWN;
            vm_file_fclose(f);
        }
        return MFX_ERR_NONE;
    }

    IppsMD5State *ctx;
    mfxU8 md5_digest[MD5_DIGEST_LENGTH];
};