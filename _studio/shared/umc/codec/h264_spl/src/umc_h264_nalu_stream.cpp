//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_h264_nalu_stream.h"

namespace UMC
{

    H264_NALU_Stream::H264_NALU_Stream()
    {
        Init( 0 );
    }

    H264_NALU_Stream::~H264_NALU_Stream(void){}

    Status H264_NALU_Stream::Init( Ipp32s p_iSize )
    {
        m_vecNALU.clear();
        m_vecNALU.reserve(p_iSize);
        return UMC_OK;
    }

    Status H264_NALU_Stream::Close()
    {
        m_vecNALU.clear();
        return UMC_OK;
    }

    Ipp32s H264_NALU_Stream::GetNALUnit(MediaData * pSource)
    {
        if (!m_code)
            m_vecNALU.clear();

        Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        if (!size)
            return 0;

        Ipp32s startCodeSize;

        Ipp32s iCodeNext = FindStartCode(source, size, startCodeSize);

        if (m_vecNALU.size())
        {
            if (!iCodeNext)
            {
                size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
                m_vecNALU.insert(m_vecNALU.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
                pSource->MoveDataPointer((Ipp32s)sz);
                return 0;
            }

            source -= startCodeSize;
            m_vecNALU.insert(m_vecNALU.end(), (Ipp8u *)pSource->GetDataPointer(), source);
            pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));

            Ipp32s code = m_code;
            m_code = 0;

            return code;
        }

        if (!iCodeNext)
        {
            pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));
            return 0;
        }

        m_code = iCodeNext;

        // move before start code
        pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize));

        Ipp32s startCodeSize1;
        iCodeNext = FindStartCode(source, size, startCodeSize1);

        //pSource->MoveDataPointer(startCodeSize);

        if (!iCodeNext)
        {
            VM_ASSERT(!m_vecNALU.size());
            size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
            m_vecNALU.insert(m_vecNALU.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
            pSource->MoveDataPointer((Ipp32s)sz);
            return 0;
        }

        // fill
        size_t nal_size = source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize1;
        m_vecNALU.insert(m_vecNALU.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + nal_size);
        pSource->MoveDataPointer((Ipp32s)nal_size);

        Ipp32s code = m_code;
        m_code = 0;
        return code;
    }

    Ipp32s H264_NALU_Stream::EndOfStream(MediaData & source)
    {
        if (!m_code)
        {
            m_vecNALU.clear();
            return 0;
        }

        if (m_vecNALU.size())
        {
            m_vecNALU.insert(m_vecNALU.end(), (Ipp8u *)source.GetDataPointer(), (Ipp8u *)source.GetDataPointer() + source.GetDataSize());
            source.MoveDataPointer((Ipp32s)source.GetDataSize());
            Ipp32s code = m_code;
            m_code = 0;
            return code;
        }

        m_code = 0;
        return 0;
    }

    Ipp32s H264_NALU_Stream::FindStartCode(Ipp8u * (&pb), size_t & size, Ipp32s & startCodeSize)
    {
        Ipp32u zeroCount = 0;

        for (Ipp32u i = 0 ; i < (Ipp32u)size; i++, pb++)
        {
            switch(pb[0])
            {
            case 0x00:
                zeroCount++;
                break;
            case 0x01:
                if (zeroCount >= 2)
                {
                    startCodeSize = IPP_MIN(zeroCount + 1, 4);
                    size -= i + 1;
                    pb++; // remove 0x01 symbol
                    zeroCount = 0;
                    if (size >= 1)
                    {
                        return pb[0] & 0x1F; // NAL unit type
                    }
                    else
                    {
                        pb -= startCodeSize;
                        size += startCodeSize;
                        startCodeSize = 0;
                        return 0;
                    }
                }
                zeroCount = 0;
                break;
            default:
                zeroCount = 0;
                break;
            }
        }

        zeroCount = IPP_MIN(zeroCount, 3);
        pb -= zeroCount;
        size += zeroCount;
        zeroCount = 0;
        startCodeSize = 0;
        return 0;
    }

    Status H264_NALU_Stream::PutData(MediaData & p_dataNal)
    {
        Ipp32s iCode = GetNALUnit(&p_dataNal);

        if (iCode == 0)
            return UMC_ERR_NOT_ENOUGH_DATA;

        return UMC_OK;
    }

    Status H264_NALU_Stream::LockOutputData(MediaData & o_dataNALU)
    {
        if( 0 == m_vecNALU.size() ){
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        o_dataNALU.SetBufferPointer(&(m_vecNALU[0]), m_vecNALU.size());
        o_dataNALU.SetDataSize(m_vecNALU.size());
        return UMC_OK;
    }

    Status H264_NALU_Stream::UnLockOutputData()
    {
        m_vecNALU.clear();
        return UMC_OK;
    }
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
