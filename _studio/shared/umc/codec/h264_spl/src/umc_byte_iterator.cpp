//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

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
