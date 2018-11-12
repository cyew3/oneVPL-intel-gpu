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

#ifndef __UMC_BYTE_ITERATOR_H__
#define __UMC_BYTE_ITERATOR_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "ippdefs.h"
#include "umc_structures.h"
#include "umc_media_data.h"

namespace UMC
{

    /**
     * The byte iterator with H.264 prevention support
     */
    class PrevH264ByteIterator
    {
    private:

        typedef enum PrevH264State
        {
            H264_PREV_NONE  = 0,
            H264_PREV_00    = 1,
            H264_PREV_00_00 = 2
        } PrevH264State;

    public:

        PrevH264ByteIterator(MediaData & p_data);
        virtual ~PrevH264ByteIterator();

        Status Increment(Ipp8u & o_byte);

    private:
        MediaData & m_data;
        PrevH264State m_state;
        Ipp32s m_iStart;

        // we lock the assignment operator to avoid any
        // accasional assignments
        PrevH264ByteIterator & operator = (PrevH264ByteIterator &)
        {
            return *this;
        }
    };
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
#endif // __UMC_BYTE_ITERATOR_H__
