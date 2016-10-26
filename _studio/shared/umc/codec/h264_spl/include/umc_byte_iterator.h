//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

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
