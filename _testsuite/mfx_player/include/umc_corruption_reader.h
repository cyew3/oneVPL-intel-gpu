/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __UMC_CORRUPTOION_READER_H__
#define __UMC_CORRUPTOION_READER_H__

#include "umc_defs.h"
#define UMC_ENABLE_FILE_CORRUPT_DATA_READER

#if defined (UMC_ENABLE_FILE_CORRUPT_DATA_READER)

#include "umc_file_reader.h"

namespace UMC
{
    //HRESULT GetRegString(const vm_char *pSubKey, vm_char *pString, int cbString);
    //HRESULT SetRegString(const vm_char *pSubKey, vm_char *pString);
    #define LOST_FREQUENCY 30
    #define PACK_SIZE      188

    enum CorruptionLevel
    {
        CORRUPT_NONE             = 0x0000,
        CORRUPT_CASUAL_BITS      = 0x0001, //CorruptData_CasualBits
        CORRUPT_DATA_PACKETS     = 0x0002,
        CORRUPT_EXCHANGE_PACKETS = 0x0004
    };

    class CorruptionReaderParams : public DataReaderParams
    {
        DYNAMIC_CAST_DECL(CorruptionReaderParams, DataReaderParams);
    public:
        DataReader       * pActual;
        DataReaderParams * pActualParams;
        Ipp32u             CorruptMode;
        Ipp32u             nPacketSize;
        Ipp32u             nLostFrequency;

        CorruptionReaderParams()
            : pActual()
            , pActualParams()
            , CorruptMode()
            , nPacketSize(PACK_SIZE)
            , nLostFrequency(LOST_FREQUENCY)
        {
        }
    };

    class CorruptionReader : public DataReader
    {
        DYNAMIC_CAST_DECL(CorruptionReader, DataReader);
    public:
        Status      Init(DataReaderParams *InitParams);
        Status      Close();
        Status      Reset();

        virtual ~CorruptionReader();
        CorruptionReader();

        //lost part of package
        Status CorruptData_Packages(void *pData, Ipp32u *nsize);
        //rearrangement of 2 neighbour packages
        Status CorruptData_ExchangePackages(void *pData, Ipp32u* /*nsize*/);
        //lost casual bytes
        Status CorruptData_CasualBits(void *pData, Ipp32u *nsize);

        Status GetData(void *data, Ipp32u *nsize);
        Status ReadData(void *pData, Ipp32u *nsize);

        Status MovePosition(Ipp64u npos);
        Status CacheData(void *data, Ipp32u *nsize, Ipp32s how_far);
        Status SetPosition(Ipp64f pos);
        Ipp64u GetPosition();
        Ipp64u GetSize();
    
    protected:
        CorruptionReaderParams  m_Init;

        Ipp32u ReadDataSize;
        Ipp32u CurPackNum;
        Ipp32u PrevPackNum;

        Ipp32u LostDataSize;
        Ipp32u LostPackNum; //number of packege in witch LostDataSize will be lost

        //package exchange
        Ipp32u ExchangePackLeft;        
        Ipp32u ExchangePackRight;  
    };
} //namespace UMC
#endif //UMC_ENABLE_FILE_CORRUPT_DATA_READER
#endif /* __UMC_CORRUPTOION_READER_H__ */
