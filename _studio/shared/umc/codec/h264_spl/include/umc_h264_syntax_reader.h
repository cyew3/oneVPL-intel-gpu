//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

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
