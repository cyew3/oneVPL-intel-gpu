//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_FIO_READER)

#include <string.h>
#include "umc_fio_reader.h"

UMC::FIOReader::FIOReader():
    m_pFile(NULL),
    m_stFileSize(0),
    m_stPos(0)
{}

UMC::FIOReader::~FIOReader()
{
    Close();
}

UMC::Status UMC::FIOReader::Init(DataReaderParams *pInit)
{
    UMC::Status umcRes = UMC_OK;

    FileReaderParams* pParams = DynamicCast<FileReaderParams, DataReaderParams>(pInit);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    m_pFile = vm_file_open(pParams->m_file_name, VM_STRING("rb"));
    if (NULL == m_pFile)
        return UMC_ERR_FAILED;

    if (0 == vm_file_fseek(m_pFile, 0, VM_FILE_SEEK_END))
    {
        unsigned long long lRes = vm_file_ftell(m_pFile);
        if (-1 != static_cast<int>(lRes))
        {
            m_stFileSize = (unsigned long long)lRes;
        }

        if (0 != vm_file_fseek(m_pFile, 0, VM_FILE_SEEK_SET))
        {
            return UMC_ERR_FAILED;
        }
    }

    return umcRes;
}

UMC::Status UMC::FIOReader::Close()
{
    if (NULL != m_pFile)
    {
        vm_file_fclose(m_pFile);
        m_pFile = NULL;
    }

    m_stFileSize = 0;
    m_stPos = 0;
    return UMC_OK;
}

UMC::Status UMC::FIOReader::Reset()
{
    if (!m_pFile)
        return UMC_ERR_NOT_INITIALIZED;

    if (0 != vm_file_fseek(m_pFile, 0, VM_FILE_SEEK_SET))
        return UMC_ERR_FAILED;

    m_stPos = 0;
    return UMC_OK;
}

UMC::Status UMC::FIOReader::ReadData(void *data, uint32_t *nsize)
{
    if (!m_pFile)
        return UMC_ERR_NOT_INITIALIZED;

    size_t stRes = vm_file_fread(data, 1, *nsize, m_pFile);
    if (stRes < *nsize)
    {
        if (vm_file_feof(m_pFile))
        {
            *nsize = (uint32_t) stRes;
            return UMC_ERR_END_OF_STREAM;
        }
        else
        {
            *nsize = 0;
            return UMC_ERR_FAILED;
        }
    }

    m_stPos = vm_file_ftell(m_pFile);

    return UMC_OK;
}

UMC::Status UMC::FIOReader::CacheData(void *data, uint32_t *nsize, int32_t how_far)
{
    unsigned long long stInitialPos = m_stPos;

    if (!m_pFile)
        return UMC_ERR_NOT_INITIALIZED;

    if (0 != vm_file_fseek(m_pFile, how_far, VM_FILE_SEEK_CUR))
        return UMC_ERR_FAILED;

    Status umcRes = GetData(data, nsize);

    if (UMC_OK == umcRes || UMC_ERR_END_OF_STREAM == umcRes)
    {
        if (0 != vm_file_fseek(m_pFile, static_cast<long>(stInitialPos), VM_FILE_SEEK_SET))
            return UMC_ERR_FAILED;
        else
            m_stPos = stInitialPos;
    }
    return umcRes;
}

UMC::Status UMC::FIOReader::MovePosition(unsigned long long npos)
{
    if (!m_pFile)
        return UMC_ERR_NOT_INITIALIZED;

    if (0 != npos)
    {
        if (0 != vm_file_fseek(m_pFile, (long)npos, VM_FILE_SEEK_CUR))
            return UMC_ERR_FAILED;

        m_stPos = vm_file_ftell(m_pFile);
    }
    return UMC_OK;
}

unsigned long long UMC::FIOReader::GetPosition()
{
    return m_stPos;
}

UMC::Status UMC::FIOReader::SetPosition(double position)
{
    if (!m_pFile)
        return UMC_ERR_NOT_INITIALIZED;

    long lOffset = (long) (((long long) m_stFileSize) * position);
    if (0 != vm_file_fseek(m_pFile, lOffset, VM_FILE_SEEK_SET))
        return UMC_ERR_FAILED;

    m_stPos = vm_file_ftell(m_pFile);

    return UMC_OK;
}

unsigned long long UMC::FIOReader::GetSize()
{
    return m_stFileSize;
}

#endif /* UMC_ENABLE_FIO_READER */
