//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

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
