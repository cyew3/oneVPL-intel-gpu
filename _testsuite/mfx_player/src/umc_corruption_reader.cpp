/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
#include "umc_defs.h"

#define UMC_ENABLE_FILE_CORRUPT_DATA_READER

#if defined (UMC_ENABLE_FILE_CORRUPT_DATA_READER)

#include <math.h>
#include "umc_corruption_reader.h" 
#include "assert.h"
#include <stdlib.h>
#include <time.h>

namespace UMC {


CorruptionReader::CorruptionReader()
{
    LostPackNum       = 0;
    LostDataSize      = 0;
    CurPackNum        = 0;
    ReadDataSize      = 0;
    ExchangePackLeft  = 0;        
    ExchangePackRight = 0;  
}

Status CorruptionReader::Init(DataReaderParams *pInitParams)
{
    Status sts = UMC_OK;

    CorruptionReaderParams *pCoruptionParams = DynamicCast<CorruptionReaderParams>(pInitParams);
    
    if (NULL == pCoruptionParams || NULL == pCoruptionParams->pActual)
        return UMC_ERR_INVALID_PARAMS;

    m_Init = *pCoruptionParams;

    sts = m_Init.pActual->Init(m_Init.pActualParams);

    //for CorruptData_Packages
    LostPackNum   = (Ipp32u)(rand()*m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX)+1));
    LostDataSize  = (Ipp32u)(rand()*m_Init.nPacketSize/(static_cast<unsigned int>(RAND_MAX)+1));

    //for CorruptData_ExchangePackages
    ExchangePackLeft  = (Ipp32u)(rand()*m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX)+1));
    ExchangePackRight = ExchangePackLeft + 1;  

    CurPackNum      = 0;
    PrevPackNum     = 0;
    ReadDataSize    = 0;

    return sts;
}

Status CorruptionReader::Close()
{
    Status sts = UMC_OK;

    sts = Reset();

    if(NULL != m_Init.pActual)
        sts = m_Init.pActual->Close();

    return sts;
}

CorruptionReader::~CorruptionReader()
{
    Close();

    UMC_DELETE(m_Init.pActual);
    UMC_DELETE(m_Init.pActualParams);
}

Status CorruptionReader::Reset()
{
    Status sts = (NULL == m_Init.pActual) ? UMC_OK : m_Init.pActual->Reset();

    LostPackNum       = (Ipp32u)(rand()*m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX)+1));
    LostDataSize      = (Ipp32u)(rand()*m_Init.nPacketSize/(static_cast<unsigned int>(RAND_MAX)+1));
    CurPackNum        = 0;
    ReadDataSize      = 0;
    PrevPackNum       = 0;
    ExchangePackLeft  = (Ipp32u)(rand()*m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX)+1));
    ExchangePackRight = ExchangePackLeft + 1;  

    return sts;
}

Status CorruptionReader::ReadData(void *pData, Ipp32u *nsize)
{
    return GetData(pData, nsize);
}

Status CorruptionReader::GetData(void *data, Ipp32u *nsize)
{
    Status sts = UMC_OK;

    if (NULL == m_Init.pActual)
        return UMC_ERR_NULL_PTR;

    sts = m_Init.pActual->GetData(data, nsize);

    if(!(sts == UMC_OK || sts == UMC_ERR_END_OF_STREAM) || *nsize == 0)
        return sts;

    CurPackNum = (ReadDataSize + *nsize)/m_Init.nPacketSize;

    vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("PrevPackNum = %d\n"),  PrevPackNum);
    vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("CurPackNum = %d\n"),   CurPackNum);
    vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("LostPackNum = %d\n"),  LostPackNum);
    vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("LostDataSize = %d\n"), LostDataSize);
    vm_debug_trace1(VM_DEBUG_INFO, VM_STRING("ReadDataSize = %d\n"), ReadDataSize);

    if(m_Init.CorruptMode & CORRUPT_CASUAL_BITS)
       CorruptData_CasualBits(data, nsize);

    if(m_Init.CorruptMode & CORRUPT_DATA_PACKETS)
       CorruptData_Packages(data, nsize);

    if(m_Init.CorruptMode & CORRUPT_EXCHANGE_PACKETS)
       CorruptData_ExchangePackages(data, nsize);

    ReadDataSize += *nsize;
    PrevPackNum   =  CurPackNum;

    if(ReadDataSize > m_Init.nLostFrequency * m_Init.nPacketSize)
    {
        ReadDataSize     -= m_Init.nLostFrequency * m_Init.nPacketSize;
        CurPackNum        = ReadDataSize/m_Init.nPacketSize;
        PrevPackNum       = CurPackNum;
        LostPackNum       = (Ipp32u)(rand() * m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX) + 1));
        LostDataSize      = (Ipp32u)(rand() * m_Init.nPacketSize   /(static_cast<unsigned int>(RAND_MAX) + 1));
        ExchangePackLeft  = (Ipp32u)(rand() * m_Init.nLostFrequency/(static_cast<unsigned int>(RAND_MAX) + 1));
        ExchangePackRight = ExchangePackLeft + 1;  
    }

    return sts;
}

Status CorruptionReader::CorruptData_Packages(void *pData, Ipp32u *nsize)
{
    Status sts = UMC_OK;
    Ipp32u pos = 0;
    Ipp32u i   = 0;
    Ipp8u* ptr = (Ipp8u*)pData;
    Ipp32u shift = 0;

    if(CurPackNum > LostPackNum && LostPackNum > PrevPackNum)
    {
        if(ReadDataSize + *nsize > (LostPackNum) * m_Init.nPacketSize)
        {
            pos = LostPackNum * m_Init.nPacketSize - ReadDataSize;
            assert(pos < *nsize);
            shift = (Ipp32u)(rand() * (m_Init.nPacketSize - LostDataSize)/ (static_cast<unsigned int>(RAND_MAX)+1));
            pos += shift;
            assert(*nsize - LostDataSize - pos < *nsize);
            for(i = 0; i < *nsize - LostDataSize - pos; i++)
            {
                ptr[pos + i] = ptr[pos + i + LostDataSize];
            }
            *nsize = *nsize - LostDataSize;
        }
        else
        {
            *nsize -= LostDataSize;
        }
    }

    return sts;
}


Status CorruptionReader::CorruptData_ExchangePackages(void *pData, Ipp32u* /*nsize*/)
{
    Status sts = UMC_OK;
    Ipp32u i = 0;
    Ipp32u pos = 0;
    Ipp8u temp = 0;
    Ipp8u* ptr = (Ipp8u*)pData;

    if(CurPackNum > ExchangePackRight &&  PrevPackNum < ExchangePackLeft)
    {
        pos = ExchangePackLeft * m_Init.nPacketSize - ReadDataSize;
        for(i = 0; i < m_Init.nPacketSize; i++)
        {
            temp = ptr[pos + i];
            ptr[pos + i] = ptr[pos + i + m_Init.nPacketSize];
            ptr[pos + i + m_Init.nPacketSize] = temp;
        }
    }

    return sts;
}

Status CorruptionReader::CorruptData_CasualBits(void *pData, Ipp32u *nsize)
{
    Status sts = UMC_OK;
    Ipp32u breaksNum  = 0;
    Ipp8u value      = 0;
    Ipp32u pos        = 0;
    Ipp32u i          = 0;
    Ipp32u frames     = 1;
    Ipp32u frameSize  = 1024 * 32;
    Ipp8u  *pFrame    = 0;
    Ipp32u  size      = 0;
    Ipp32u  done      = 0;
    
    frames = *nsize / frameSize;
    frames = frames ? frames : 1;
    size = *nsize;
    if ( size < frameSize )
        frameSize = *nsize;
    
    pFrame = (Ipp8u*)pData;
    do {
        if ( size - done < frameSize ){
            frameSize = size - done;
        }

        breaksNum = rand()%7;
        for(i = 0; i < breaksNum; i++)
        {
            pos   = rand()%(frameSize);
            assert(pos < (frameSize));
            value = (Ipp8u)(rand()% 128);
            assert(value < 128);
            pFrame[pos] = value;
        }
        pFrame += frameSize;
        done += frameSize;
    } while (--frames);   

    return sts;
}

Status CorruptionReader::MovePosition(Ipp64u npos)
{
    if (NULL == m_Init.pActual)
        return UMC_ERR_NULL_PTR;

    return m_Init.pActual->MovePosition(npos);
}
Status CorruptionReader::CacheData(void *data, Ipp32u *nsize, Ipp32s how_far)
{
    if (NULL == m_Init.pActual)
        return UMC_ERR_NULL_PTR;

    return m_Init.pActual->CacheData(data, nsize, how_far);
}

Status CorruptionReader::SetPosition(Ipp64f pos)
{
    if (NULL == m_Init.pActual)
        return UMC_ERR_NULL_PTR;

    return m_Init.pActual->SetPosition(pos);
}

Ipp64u CorruptionReader::GetPosition()
{
    if (NULL == m_Init.pActual)
        return 0;

    return m_Init.pActual->GetPosition();
}

Ipp64u CorruptionReader::GetSize()
{
    if (NULL == m_Init.pActual)
        return 0;

    return m_Init.pActual->GetSize();
}

} // namespace UMC {

#endif //UMC_ENABLE_FILE_CORRUPT_DATA_READER