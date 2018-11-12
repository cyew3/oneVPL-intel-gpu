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

#ifndef __UMC_FILE_WRITER_H__
#define __UMC_FILE_WRITER_H__

#include "umc_defs.h"

#if defined (UMC_ENABLE_FILE_WRITER)

#include <stdio.h>

#include "umc_data_writer.h"
#include "vm_file.h"

//#define USE_FILE_MAPPING

namespace UMC
{
    class FileWriterParams : public DataWriterParams
    {
        DYNAMIC_CAST_DECL(FileWriterParams, DataWriterParams)
    public:
        vm_char m_file_name[MAXIMUM_PATH];
        unsigned long long  m_portion_size;

        FileWriterParams();
    };

    class FileWriter : public DataWriter
    {
    public:
        //////////////////////////////////////////////////////////////////////////////
        //  Name:           Init
        //
        //  Purpose:        Create and map first portion from file
        //
        //  Parameters:
        //    InitParams    Pointer to the init (for filereader it's
        //                  vm_char m_file_name[] and m_portion_size
        //
        //
        //  Return:
        //    OK
        //    ERR_FILECREATE_IS_INVALID     file not ctrate
        //    ERR_NULL_PTR              pointer to the buffer is NULL
        //
        //
        //  Notes:      default value of portion_size is 0. After open portion = 1
        //
        ////////////////////////////////////////////////////////////////////////////////
        Status Init(DataWriterParams *InitParams);

        //////////////////////////////////////////////////////////////////////////////
        //  Name:           Close
        //
        //  Purpose:        Close file
        //
        //  Parameters:
        //
        //  Return:
        //    OK        file was close
        //
        //  Notes:
        //
        ////////////////////////////////////////////////////////////////////////////////
        Status Close();

        //////////////////////////////////////////////////////////////////////////////
        //  Name:           Reset
        //
        //  Purpose:        Reset all internal parameters
        //
        //  Parameters:
        //
        //  Return:
        //    OK
        //
        //  Notes:
        //
        ////////////////////////////////////////////////////////////////////////////////
        Status Reset();

        //////////////////////////////////////////////////////////////////////////////
        //  Name:           GetData
        //
        //  Purpose:        Get nsize bytes and copy to data (return number bytes which was copy)
        //
        //  Parameters:
        //    data          pointer to the data where copy nsize byte from stream
        //    nsize         integer - number bytes
        //
        //  Return:
        //    return number bytes which was copy
        //
        //  Notes:
        //
        ////////////////////////////////////////////////////////////////////////////////
        Status PutData(void *data, int32_t &nsize);

#ifndef USE_FILE_MAPPING
        //////////////////////////////////////////////////////////////////////////////
        //  Name:           SetPosition
        //
        //  Purpose:        Set current position in destination media
        //
        //  Parameters:
        //    nPosLow       Lower 32 bit(s) of position
        //    lpnPosHigh    Pointer to upper 32 bit(s) of position (can be NULL)
        //    nMethod       Initial position. Use C standard defines:
        //                  SEEK_CUR - Current position of pointer
        //                  SEEK_END - End of media
        //                  SEEK_SET - Beginning of media
        //
        //  Return:
        //    return UMC_OK if position is changed, otherwise code of error
        //
        //  Notes:
        //    Not all media can change current position
        ////////////////////////////////////////////////////////////////////////////////
        virtual Status SetPosition(uint32_t nPosLow, uint32_t *lpnPosHigh, uint32_t nMethod);

        //////////////////////////////////////////////////////////////////////////////
        //  Name:           GetPosition
        //
        //  Purpose:        Get current position in destiantion media
        //
        //  Parameters:
        //    lpnPosLow     Pointer to variable to lower 32 bit(s) of position
        //    lpnPosHigh    Pointer to variable to upper 32 bit(s) of position
        //
        //  Return:
        //    return UMC_OK if position is returned, otherwise code of error
        //
        //  Notes:
        //    Not all media can return current position
        ////////////////////////////////////////////////////////////////////////////////
        virtual Status GetPosition(uint32_t *lpnPosLow, uint32_t *lpnPosHigh);
#endif //USE_FILE_MAPPING

        FileWriter();
        virtual ~FileWriter();

    protected:

#ifndef USE_FILE_MAPPING
        vm_file * m_file;
#else
        Status FileWriter::MapCSize();
        vm_mmap      m_mmap;

        uint8_t       *m_pbBuffer;
        uint32_t       m_uiFileSize;
        uint32_t       m_uiPortionSize;
        unsigned long long       m_stDoneSize;               // accumulative size of processed portions
        unsigned long long       m_stPos;                    // position in current portion of file
        uint32_t       m_uiPageSize;
        bool         m_bBufferInit;
#endif
    };

}//namespace UMC

#endif //(UMC_ENABLE_FILE_READER)

#endif /* __UMC_FILE_WRITER_H__ */
