// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __UMC_MPEG2PS_PARSER_H__
#define __UMC_MPEG2PS_PARSER_H__

#include "umc_stream_parser.h"

namespace UMC
{
    class Mpeg2PsParser : public Mpeg2PesParser
    {
        DYNAMIC_CAST_DECL(Mpeg2PsParser, Mpeg2PesParser)
    public:
        virtual Status Init(StreamParserParams &rInit);
        virtual Status Close(void);
        virtual Status CheckNextData(MediaData* data, Ipp32u* pTrack);
        virtual Status SetPosition(Ipp64u bytePos);
        virtual Status GetSystemTime(CheckPoint &rCheckPoint, Ipp64u upToPos);

    protected:
        Status ReSync(void);
        Ipp32s GetTrackByPidOrCreateNew(Ipp32s iPid, bool *pIsNew);
        Status ParsePsPmt(Mpeg2TsPmt &pmt, Ipp32s &iPos, bool bDetectChanges);
        Status ParsePsPack(Ipp32s &iPos);

        // used if Program Stream map presents
        Mpeg2TsPmt m_Pmt;
    };
}

#endif /* __UMC_MPEG2PS_PARSER_H__ */
