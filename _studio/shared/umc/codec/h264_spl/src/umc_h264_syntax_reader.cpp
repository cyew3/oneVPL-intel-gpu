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

#include "umc_h264_syntax_reader.h"

namespace UMC
{
    H264_SyntaxReader::H264_SyntaxReader( MediaData & p_data )
        : PrevH264ByteIterator( p_data )
        , m_cData(0)
    {
        m_iBitOffset = 8;
    }

    H264_SyntaxReader::~H264_SyntaxReader(void){}

    Status H264_SyntaxReader::LoadByte()
    {
        if(m_iBitOffset > 7){
            m_iBitOffset = 0;
            Status status = Increment(m_cData);
            if( UMC_OK != status){
                return status;
            }
        }
        return UMC_OK;
    }

    Ipp32s H264_SyntaxReader::GetBitIncrement()
    {
        Ipp32s iRes = ((m_cData >> ( 7 - m_iBitOffset )) & 0x01 );
        m_iBitOffset ++;
        return iRes;
    }

    Status H264_SyntaxReader::CountGolombLength(Ipp32s & o_iLength)
    {
        Ipp32s iCur = 0,
            iLength = -1;
        do{
            if( UMC_OK != LoadByte()){
                return UMC_ERR_FAILED;
            }
            iCur = GetBitIncrement();
            iLength ++;
        } while( 0== iCur);

        o_iLength = iLength;
        return UMC_OK;
    }

    Status H264_SyntaxReader::ReadU(Ipp32u p_iWidth, Ipp32u & o_int)
    {
        Ipp32u iRes = 0;
        for(Ipp32s i=0; i<(Ipp32s)p_iWidth; i++){
            if( UMC_OK != LoadByte()){
                return UMC_ERR_FAILED;
            }
            iRes = (Ipp32u)(iRes << 1);
            iRes += GetBitIncrement();
        }
        o_int = iRes;
        return UMC_OK;
    }

    Status H264_SyntaxReader::ReadUE(Ipp32u & o_int)
    {
        o_int = 0;
        Ipp32s iLength = 0;
        if( UMC_OK != CountGolombLength( iLength ) ){
            return UMC_ERR_FAILED;
        }

        if( 0 == iLength){
            o_int = 0;
            return UMC_OK;
        }

        Ipp32u iValue = 0;
        if( UMC_OK != ReadU(iLength, iValue) ){
            return UMC_ERR_FAILED;
        }
        Ipp32u iResult = (1 << iLength) - 1 + iValue;
        o_int = iResult;
        return UMC_OK;
    }

    Status H264_SyntaxReader::ReadSE(Ipp32s & o_int)
    {
        Ipp32u iValue;
        if( UMC_OK != ReadUE(iValue)){
            return UMC_ERR_FAILED;
        }

        Ipp32u parity = iValue%2;
        Ipp32s iRes = (iValue/2 + ((parity == 1) ? 1 : 0));

        if( 0 == parity){
            iRes *= (-1);
        }
        o_int = iRes;
        return UMC_OK;
    }
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
