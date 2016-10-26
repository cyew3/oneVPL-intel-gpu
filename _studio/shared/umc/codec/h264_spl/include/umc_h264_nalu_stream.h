//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

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

