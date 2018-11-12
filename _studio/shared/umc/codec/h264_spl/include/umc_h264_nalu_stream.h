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

#ifndef __UMC_H264_NALU_STREAM_H__
#define __UMC_H264_NALU_STREAM_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_structures.h"
#include "umc_media_data.h"

#include <vector>

namespace UMC
{
    class H264_NALU_Stream
    {
    public:
        H264_NALU_Stream(void);
        ~H264_NALU_Stream(void);

        /**
         * Initializes the stream
         * @param p_iSize - maximum NALU size
         */
        Status Init( Ipp32s p_iSize = 65536 );

        Status Close();

        /**
         * Looks for a complete NALU
         * @param p_dataNal - input from file
         * @retrun UMC_OK - in case a NALU found, UMC_ERR_NOT_ENOUGH_DATA - in case no NALU was found
         */
        Status PutData(MediaData & p_dataNal);
        /**
         * Acquires the internally stored NALU
         */
        Status LockOutputData(MediaData & o_dataAU);
        /**
         * Resets the internal buffer
         */
        Status UnLockOutputData();

        Ipp32s EndOfStream(MediaData & source);

    private:

        Ipp32s GetNALUnit(MediaData * pSource);
        Ipp32s FindStartCode(Ipp8u * (&pb), size_t & size, Ipp32s & startCodeSize);

        Ipp32s m_code;
        std::vector<Ipp8u> m_vecNALU;
    };
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
#endif // __UMC_H264_AU_STREAM_H__

