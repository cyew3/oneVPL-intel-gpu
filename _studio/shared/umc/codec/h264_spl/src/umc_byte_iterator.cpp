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

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_byte_iterator.h"

namespace UMC
{

    PrevH264ByteIterator::PrevH264ByteIterator(MediaData & p_data) : m_data(p_data)
    {
        m_iStart = 0;
                m_state = H264_PREV_NONE;
    }
    PrevH264ByteIterator::~PrevH264ByteIterator(){}

    Status PrevH264ByteIterator::Increment(Ipp8u & o_byte)
    {
        o_byte = 0x00;
        if((Ipp32s)m_data.GetDataSize() < 1){
            return UMC_ERR_END_OF_STREAM;
        }
        // Grabbing the first byte
        Ipp8u cByte = *((Ipp8u*)m_data.GetDataPointer());
        // Changing the parsing state accordingly
        switch(cByte)
        {
        case 0x00:
            //////////////////////////////////////////////////////////////////////////
            switch(m_state){
                case H264_PREV_NONE:
                    m_state = H264_PREV_00;
                    break;
                case H264_PREV_00:
                    m_state = H264_PREV_00_00;
                    break;
                default:
                    m_state = H264_PREV_NONE;
                    break;
            }
            //////////////////////////////////////////////////////////////////////////
            break;
        case 0x03:
            //////////////////////////////////////////////////////////////////////////
            switch(m_state){
                case H264_PREV_00_00:
                    m_data.MoveDataPointer(1);
                    if((Ipp32s)m_data.GetDataSize() < 1) {
                        return UMC_ERR_END_OF_STREAM;
                    }
                    cByte = *((Ipp8u*)m_data.GetDataPointer());
                    m_state = H264_PREV_NONE;
                    break;
                default:
                    m_state = H264_PREV_NONE;
                    break;
            }
            //////////////////////////////////////////////////////////////////////////
            break;
        default:
            m_state = H264_PREV_NONE;
            break;
        }

        o_byte = cByte;
        m_data.MoveDataPointer(1);
        return UMC_OK;
    }
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
