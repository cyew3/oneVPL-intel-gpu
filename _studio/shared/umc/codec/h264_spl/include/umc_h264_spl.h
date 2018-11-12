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

#ifndef __UMC_H264_SPLITTER_H__
#define __UMC_H264_SPLITTER_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_structures.h"
#include "umc_splitter.h"
#include "umc_media_data.h"
#include "umc_h264_au_stream.h"
#include "umc_h264_nalu_stream.h"

#include <memory>

namespace UMC
{
    /**
     * Imitates the splitter fucntionality
     */
    class H264Splitter : public Splitter
    {
    public:

        H264Splitter();
        virtual ~H264Splitter();

        virtual Status Init(SplitterParams &initParams);

        virtual Status Close();

        virtual Status GetNextData(MediaData *pDst, Ipp32u);

        virtual Status Run() {return UMC_OK;};

        virtual Status Stop(void) {return UMC_OK;};

        virtual Status SetPosition(Ipp64f /*fPosition*/) {return UMC_OK;}

        using Splitter::GetPosition;
        virtual Status GetPosition(Ipp64f &/*fPosition*/) {return UMC_OK;}

    protected:

        /**
         * Initializes internal state
         */
        void Init();

        /**
         * Loads the next chunk of the bytestream
         */
        virtual Status LoadByteData(MediaData & p_data);

        /**
         * Returns the next NALU from the bytestream
         */
        Status GetNALUData(MediaData & p_data);

        /**
         * Returns whether the NALU stream has more binary data to parse
         */
        bool HasMoreBinData();

    private:
        // File buffer
        MediaData m_dataFile;

        enum
        {
            BUFFER_SIZE = 262144
        };

        // The buffer for the media data
        Ipp8u m_aFileBuffer[BUFFER_SIZE];

    protected:
        // nalu stream
        H264_NALU_Stream m_streamNALU;
        // au stream
        H264_AU_Stream m_streamAU;
        // Indicates wether we had sent all AU units
        bool m_bEndOfAUSeen;
        // Indicates weather the source file eof seen
        bool m_bEndOfSourceSeen;
    };
}

#endif // UMC_ENABLE_H264_SPLITTER
#endif //  __UMC_H264_HELPER_H__
