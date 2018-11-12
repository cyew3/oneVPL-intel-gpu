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

#ifndef __UMC_H264_SYNTAX_READER_H__
#define __UMC_H264_SYNTAX_READER_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_structures.h"
#include "umc_media_data.h"
#include "umc_byte_iterator.h"

namespace UMC
{
    /**
     * The generic syntax reader for H.264
     */
    class H264_SyntaxReader : public PrevH264ByteIterator
    {

    public:
        H264_SyntaxReader( MediaData & p_data ) ;
        virtual ~H264_SyntaxReader(void);

    private:

        Status LoadByte();
        Ipp32s GetBitIncrement();
        Status CountGolombLength(Ipp32s & o_iLength);

    public:

        /**
         * Reads u(v)
         */
        Status ReadU(Ipp32u p_iWidth, Ipp32u & o_int);

        /**
        * Reads ue(v)
        */
        Status ReadUE(Ipp32u & o_int);

        /**
        * Reads se(v)
        */
        Status ReadSE(Ipp32s & o_int);

    private:
        // bit offset in the m_cData byte
        Ipp32u m_iBitOffset;
        // currently parsed byte
        Ipp8u m_cData;

    };
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
#endif // __UMC_H264_SYNTAX_READER_H__
