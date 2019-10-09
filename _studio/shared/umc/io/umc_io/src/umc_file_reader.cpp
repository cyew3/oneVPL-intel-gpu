// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#include "umc_file_reader.h"
#include "mfx_utils.h"

#if defined (UMC_ENABLE_FILE_READER) || defined (UMC_ENABLE_FIO_READER)

#include <string.h>
#include "vm_strings.h"
#include "vm_debug.h"
#include "umc_dynamic_cast.h"

using namespace UMC;

FileReader::FileReader()
    : m_mmap()
    , m_pBuffer(0)
    , m_iPageSize(vm_mmap_get_alloc_granularity())
    , m_iFileSize(0)
    , m_iOff(0)
    , m_iPortion(0)
{
    vm_mmap_set_invalid(&m_mmap);
}

FileReader::~FileReader()
{
    Close();
}

Status FileReader::Init(DataReaderParams *pInit)
{
    FileReaderParams* pParams = DynamicCast<FileReaderParams, DataReaderParams>(pInit);

    UMC_CHECK(pParams, UMC_ERR_INIT)
    UMC_CHECK((m_iPageSize > 0),  UMC_ERR_INIT)
    m_iPortion = m_iPageSize * ((pParams->m_portion_size <= 0) ? 188 : ((m_iPortion + m_iPageSize - 1) / m_iPageSize));
    UMC_CHECK((m_iPortion > 0),  UMC_ERR_INIT)

    if (vm_mmap_is_valid(&m_mmap))
        Close();

    m_iFileSize = vm_mmap_create(&m_mmap, pParams->m_file_name, FLAG_ATTRIBUTE_READ);

    if (0 == m_iFileSize) {
        vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("File %s open error\n"), pParams->m_file_name);
        vm_mmap_set_invalid(&m_mmap);
        return UMC_ERR_OPEN_FAILED;
    }

    return UMC_OK;
}

Status FileReader::Close()
{
    vm_mmap_close(&m_mmap);
    vm_mmap_set_invalid(&m_mmap);
    m_pDataPointer = 0;
    m_pEODPointer  = 0;
    m_iFileSize    = 0;
    m_iOff         = 0;
    m_iPortion     = 0;
    return UMC_OK;
}

Status FileReader::Reset()
{
    UMC_CHECK(vm_mmap_is_valid(&m_mmap), UMC_ERR_NOT_INITIALIZED)
    vm_mmap_unmap(&m_mmap);
    m_pDataPointer = 0; // set to zero pointers
    m_pBuffer      = 0;
    m_pEODPointer  = 0;
    m_iOff         = 0;
    return UMC_OK;
}

// Requirements:
// 0 <= m_iFileSize
// 0 <= m_iPos <= m_iFileSize
// 0 < m_iPageSize
// 0 < m_iPortion
// m_mmap is valid
Status FileReader::OpenView(long long iSize)
{
    UMC_CHECK((iSize > (long long)(m_pEODPointer - m_pDataPointer)), UMC_OK); // already ok
    long long iPos     = m_iOff + (long long)(m_pDataPointer - m_pBuffer);
    long long iStart   = iPos - (iPos % m_iPageSize);     // align to page
    long long iMax     = m_iFileSize - iStart;            // maximum depends on file size
    unsigned long long iiStart  = iStart, iiSize;                  // for vm_mmap_set_view

    iSize += iPos - iStart;                            // increase size due align
    iSize  = mfx::clamp(iSize, (long long)m_iPortion, iMax);      // would like to open more if iSize small; cut off end of file
    iiSize = (unsigned long long) iSize;
    UMC_CHECK(((iSize - (iPos - iStart)) > (long long)(m_pEODPointer - m_pDataPointer)), UMC_OK); // ok after end of file cut

    m_pBuffer      = m_pEODPointer = m_pDataPointer =  (uint8_t *)vm_mmap_set_view(&m_mmap, &iiStart, &iiSize);
    m_iOff         = iPos; // nullify all if bad return
    UMC_CHECK(((m_pDataPointer) && (iStart == (long long)iiStart) && (iSize == (long long)iiSize)), UMC_ERR_FAILED); // sanity check
    m_iOff          = iStart; // right setup if ok
    m_pDataPointer += (iPos - iStart);
    m_pEODPointer  += iSize;

    return UMC_OK;
}

Status FileReader::ReadData(void *data, uint32_t *nsize)
{
    if (((size_t)(m_pEODPointer - m_pDataPointer)) >= *nsize)
    {
        MFX_INTERNAL_CPY(data, m_pDataPointer, *nsize);
        m_pDataPointer += *nsize;
        return UMC_OK;
    }

    Status umcRes = CacheData(data, nsize, 0);
    MovePosition(*nsize); // lazy to check all twice

    return umcRes;
}

Status FileReader::CacheData(void *data, uint32_t *nsize, int32_t how_far)
{
    if (((size_t)(m_pEODPointer - m_pDataPointer)) >= (*nsize + how_far))
    {
        std::copy(m_pDataPointer + how_far, m_pDataPointer + how_far + *nsize, (Ipp8u*)data);
        return UMC_OK;
    }
    long long iSize = *nsize, iMax;
    Status umcRes;

    *nsize = 0; // return zero if nothing read
    UMC_CHECK(vm_mmap_is_valid(&m_mmap), UMC_ERR_NOT_INITIALIZED)
        umcRes = OpenView(iSize + how_far);
    if (UMC_OK == umcRes)
    {
        umcRes = ((iSize += how_far) <= (iMax = (long long)(m_pEODPointer - m_pDataPointer))) ? UMC_OK : UMC_ERR_END_OF_STREAM;
        iSize  = std::min(iSize, iMax);
        iSize -= how_far;
        *nsize = (uint32_t)std::max(iSize, 0ll);
        if (iSize)
            std::copy(m_pDataPointer + how_far, m_pDataPointer + how_far + *nsize, (Ipp8u*)data);
    }

    return umcRes;
}

Status FileReader::MovePosition(unsigned long long npos)
{
    UMC_CHECK(vm_mmap_is_valid(&m_mmap), UMC_ERR_NOT_INITIALIZED)
    long long iOldPos = m_iOff + (long long)(m_pDataPointer - m_pBuffer);
    long long iPos = iOldPos + npos;

    if ( iOldPos == m_iFileSize && iPos > m_iFileSize )
        return UMC_ERR_END_OF_STREAM;

    iPos = mfx::clamp(iPos, 0ll, m_iFileSize);
    if ((iPos < m_iOff) || (iPos > (m_iOff + (long long)(m_pEODPointer - m_pBuffer)))) // out of view
    {
        vm_mmap_unmap(&m_mmap);
        m_pBuffer = m_pDataPointer = m_pEODPointer = 0;
        m_iOff = iPos;
    } else
        m_pDataPointer += (iPos - iOldPos); // move inside view

    return UMC_OK;
}

unsigned long long FileReader::GetPosition()
{
    return (unsigned long long)(m_iOff + (long long)(m_pDataPointer - m_pBuffer));
}

Status FileReader::SetPosition(double position)
{
    UMC_CHECK(vm_mmap_is_valid(&m_mmap), UMC_ERR_NOT_INITIALIZED)
        long long iPos = (long long)(position * (m_iFileSize + 0.5));
    long long iOldPos = m_iOff + (long long)(m_pDataPointer - m_pBuffer);

    return MovePosition((unsigned long long) (iPos - iOldPos));
}

unsigned long long FileReader::GetSize()
{
    return m_iFileSize;
}

#endif // (UMC_ENABLE_FILE_READER)

