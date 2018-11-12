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

#ifndef __UMC_MPEG2TS_PARSER_H__
#define __UMC_MPEG2TS_PARSER_H__

#include "umc_stream_parser.h"

#define TS_PACKET_SIZE 188
#define TTS_PACKET_SIZE 192

namespace UMC
{
    class Mpeg2TsParserParams : public StreamParserParams
    {
        DYNAMIC_CAST_DECL(Mpeg2TsParserParams, StreamParserParams)

    public:
        Mpeg2TsParserParams(void)
        {
            m_bDetectPSIChanges = false;
        }

        bool m_bDetectPSIChanges;
    };

    class Mpeg2TsParser : public Mpeg2PesParser
    {
        DYNAMIC_CAST_DECL(Mpeg2TsParser, Mpeg2PesParser)

    public:
        Mpeg2TsParser();
        virtual Status Init(StreamParserParams &init);
        virtual Status Close(void);
        virtual Status CheckNextData(MediaData* data, Ipp32u* pTrack);
        virtual Status SetPosition(Ipp64u pos);
        virtual Status GetSystemTime(CheckPoint &rCheckPoint, Ipp64u upToPos);
        virtual Status MoveToNextHeader(void);

    protected:
        Status ReSync(void);
        Status DispatchPacket(Packet &packet, Ipp32s &iPos, bool bExtractPcr);
        Status ParseTsPat(Mpeg2TsPat &pat, Ipp32s iPos, bool bDetectChanges);
        Status ParseTsPmt(Mpeg2TsPmt &pmt, Ipp32s iPos, bool bDetectChanges);
        Status DispatchPacketWithPid(Packet &packet, Ipp32s &iPos, bool bExtractPcr);

        // informs for PMT parser that PAT was changed
        // this flag is reseted at reposition, so problems are possible
        bool m_bPatWasChanged;
        bool m_bPmtWasChanged;
        bool m_bDetectPSIChanges;
        bool m_bPcrPresent;
        Ipp32s m_iPacketSize;
        Ipp32s m_iSysTimePid;

        // absolute position of first PES packet for every track
        Ipp64u m_uiFirstPesPos[MAX_TRACK];
        // PTS of last PES for every track
        Ipp64f m_dLastPesPts[MAX_TRACK];
        // DTS of last PES for every track
        Ipp64f m_dLastPesDts[MAX_TRACK];
        // absolute position of last PES for every track
        Ipp64u m_uiLastPesPos[MAX_TRACK];
        // in most cases it's 0, but for broken streams it could be more than 0
        // this value used for resync
        Ipp32s m_iOrig;
        // PAT from Mpeg2ts
        Mpeg2TsPat m_Pat;
    };
}

#endif /* __UMC_MPEG2TS_PARSER_H__ */
