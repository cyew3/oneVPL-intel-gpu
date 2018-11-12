// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_FILE_READER_H__
#define __UMC_FILE_READER_H__

#include "umc_defs.h"

#if defined (UMC_ENABLE_FILE_READER) || defined(UMC_ENABLE_FIO_READER)

#include "vm_mmap.h"
#include "vm_types.h"

#include "umc_data_reader.h"

namespace UMC
{
    class FileReaderParams : public DataReaderParams
    {
        DYNAMIC_CAST_DECL(FileReaderParams, DataReaderParams)

    public:
        /** Default constructor
        Sets m_file_name to empty string and m_portion_size to 0
        */
        FileReaderParams():m_portion_size(0)
        {   memset(m_file_name, 0, sizeof(m_file_name));   }

        vm_char m_file_name[MAXIMUM_PATH]; ///< file name
        uint32_t m_portion_size; ///< portion size
    };

    class FileReader : public DataReader
    {
        DYNAMIC_CAST_DECL(FileReader, DataReader)
    public:
        /**
        Create and map first portion from file
        \param InitParams Pointer to the init (for filereader it's vm_char file_name[255] and portion_size
        \retval UMC_OK
        \retval UMC_ERR_OPEN_FAILED       file was not open
        \retval UMC_ERR_INIT              wrong page size
        */
        Status      Init(DataReaderParams *InitParams);

        /**
        Close file
        \retval UMC_OK
        */
        Status      Close();

        /**
        Reset all internal parameters
        \retval UMC_OK
        \retval UMC_ERR_NOT_INITIALIZED             object was not initialize
        */
        Status      Reset();

        /**
        Read nsize bytes and copy to data (return number bytes which was copy)
        Cache data in case of small nsize
        \param data          pointer to the data where copy nsize byte from stream
        \param nsize         integer - number bytes
        \retval UMC_OK
        \retval UMC_ERR_NOT_INITIALIZED object was not initialize
        \retval UMC_ERR_END_OF_STREAM   end of stream
        \retval UMC_ERR_FAILED          can't map
        */
        Status      ReadData(void *data, uint32_t *nsize);

        /**
        Move position on npos bytes
        \param npos          integer (+/-) bytes
        \retval UMC_OK
        \retval UMC_ERR_NOT_INITIALIZED object was not initialize
        \retval UMC_ERR_END_OF_STREAM   end of stream
        */
        Status      MovePosition(unsigned long long npos);

        Status      CacheData(void *data, uint32_t *nsize, int32_t how_far);

        /**
        Set position
        \param pos double (0:1.0)
        \retval OK
        \note set position in the stream (file size * pos)
        */
        Status      SetPosition(double pos);

        /// return position in the stream
        unsigned long long      GetPosition();

        /// return file_size
        unsigned long long      GetSize();

        FileReader();
        virtual ~FileReader();

    protected:
        vm_mmap             m_mmap;
        uint8_t               *m_pBuffer;
        int32_t              m_iPageSize; // granularity size
        long long              m_iFileSize; // file size
        long long              m_iOff;      // offset of m_pBuffer
        int32_t              m_iPortion;  // desired view size

        virtual Status  OpenView(long long iSize);
    };

} // namespace UMC

#endif //(UMC_ENABLE_FILE_READER) || defined(UMC_ENABLE_FIO_READER)

#endif /* __UMC_FILE_READER_H__ */

