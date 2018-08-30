//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_DATA_READER_H__
#define __UMC_DATA_READER_H__

/*
//  Class:       DataReader
//
//  Notes:       Base abstract class of data reader. Class describes
//               the high level interface of abstract source of data.
//               All specific ( reading from file, from network, inernet, etc ) must be implemented in
//               derevied classes.
//               Splitter uses this class to obtain data
//
*/

#include "umc_structures.h"
#include "umc_dynamic_cast.h"

namespace UMC
{

class DataReaderParams
{
    DYNAMIC_CAST_DECL_BASE(DataReaderParams)

public:
    /// Default constructor
    DataReaderParams(void){}
    /// Destructor
    virtual ~DataReaderParams(void){}
};

class DataReader
{
    DYNAMIC_CAST_DECL_BASE(DataReader)
public:

    /// Default constructor
    DataReader(void)
        { m_pDataPointer = 0; m_pEODPointer = 0; m_bStop = 0;}
    /// Destructor
    virtual ~DataReader(void){}

    /// Initialization abstract source data
    virtual Status Init(DataReaderParams *InitParams) = 0;

    /// Close source data
    virtual Status Close(void) = 0;

    /// Reset all internal parameters to start reading from begin
    virtual Status Reset(void) = 0;

    /// Return 2 bytes
    Status Get16uSwap(uint16_t *data);
    /// Return 2 bytes without swap
    Status Get16uNoSwap(uint16_t *data);

    /// Return 4 bytes
    Status Get32uSwap(uint32_t *data);
    /// Return 4 bytes without swap
    Status Get32uNoSwap(uint32_t *data);

    /// Return 8 bytes
    Status Get64uSwap(unsigned long long *data);
    /// Return 8 bytes without swap
    Status Get64uNoSwap(unsigned long long *data);

    /// Return 1 byte
    Status Get8u(uint8_t *data);

    /// Get data
    Status GetData(void *data, uint32_t *nsize);

    /**
    Read nsize bytes and copy to data (return number bytes which was copy).
    Cache data in case of small nsize
    */
    virtual Status ReadData(void *data, uint32_t *nsize) = 0;

    /// Move position on npos bytes
    virtual Status MovePosition(unsigned long long npos) = 0;

    /// Check byte value
    Status Check8u(uint8_t *ret_byte, size_t how_far);
    /// Check int16_t value
    Status Check16u(uint16_t *ret_short, size_t how_far);
    /// Check uint32_t value
    Status Check32u(uint32_t *ret_long, size_t how_far);

    /// Check data
    Status CheckData(void *data, uint32_t *nsize, int32_t how_far);

    // Show data
    size_t ShowData(uint8_t **data);

    /// Cache and check data
    virtual Status CacheData(void *data, uint32_t *nsize, int32_t how_far) = 0;

    /// Obtain position in the stream
    virtual unsigned long long GetPosition(void) = 0;
    /// Obtain size in source data
    virtual unsigned long long GetSize(void) = 0;
    /// Set new position
    virtual Status SetPosition(double pos) = 0;

    /// Set new position
    Status SetPosition (unsigned long long pos)
    {
        unsigned long long curr_pos = GetPosition();
        if (pos >= curr_pos)
        {
            return MovePosition((pos-curr_pos));
        }
        else
        {
            SetPosition(0.0);

            return MovePosition(pos);
        }
    }

    Status StartReadingAfterReposition(void)
    {
        return UMC_OK;
    }

public:
    uint8_t  *m_pDataPointer;  // Pointer to the current data
    uint8_t  *m_pEODPointer;   // Pointer to the end of data

protected:
    bool m_bStop;
};

inline
Status DataReader::GetData(void *data, uint32_t *nsize)
{
    size_t data_sz = (size_t)(*nsize);
    Status umcRes = UMC_OK;

    do {
        *nsize = (uint32_t)data_sz;
        umcRes = ReadData(data, nsize);
    } while (umcRes == UMC_ERR_NOT_ENOUGH_DATA && m_bStop == false);

    return umcRes;
} // Status DataReader::GetData(void *data, uint32_t *nsize)

inline
Status DataReader::Get8u(uint8_t *data)
{
    uint32_t data_sz = 1;

    return GetData(data, &data_sz);

} // Status DataReader::Get8u(uint8_t *data)

inline
Status DataReader::Get16uNoSwap(uint16_t *data)
{
    uint32_t data_sz = 2;
    Status ret = GetData(data, &data_sz);

    *data = BIG_ENDIAN_SWAP16(*data);

    return ret;

} // Status DataReader::Get16uNoSwap(uint16_t *data)

inline
Status DataReader::Get16uSwap(uint16_t *data)
{
    uint32_t data_sz = 2;
    Status ret = GetData(data, &data_sz);

    *data = LITTLE_ENDIAN_SWAP16(*data);

    return ret;

} // Status DataReader::Get16uSwap(uint16_t *data)

inline
Status DataReader::Get32uNoSwap(uint32_t *data)
{
    uint32_t data_sz = 4;
    Status ret = GetData(data,&data_sz);

    *data = BIG_ENDIAN_SWAP32(*data);

    return ret;

} // Status DataReader::Get32uNoSwap(uint32_t *data)

inline
Status DataReader::Get32uSwap(uint32_t *data)
{
    uint32_t data_sz = 4;
    Status ret = GetData(data,&data_sz);

    *data = LITTLE_ENDIAN_SWAP32(*data);
    return ret;

} // Status DataReader::Get32uSwap(uint32_t *data)

inline
Status DataReader::CheckData(void *data, uint32_t *nsize, int32_t how_far)
{
/*    size_t data_sz = (size_t)(*nsize + how_far);

    if (((size_t)(m_pEODPointer - m_pDataPointer)) >= data_sz)
    {
        MFX_INTERNAL_CPY(data, m_pDataPointer + how_far, *nsize);
        return UMC_OK;
    }*/

    return CacheData(data, nsize, how_far);
}

inline
size_t DataReader::ShowData(uint8_t **data)
{
    *data = m_pDataPointer;
    return (size_t)(m_pEODPointer - m_pDataPointer);
}

inline
Status DataReader::Check8u(uint8_t *ret_byte, size_t how_far)
{
    uint32_t data_sz = 1;

    return CheckData(ret_byte, &data_sz, (int32_t)how_far);

} // Status DataReader::Check8u(uint8_t *ret_byte, size_t how_far)

inline
Status DataReader::Check16u(uint16_t *ret_short, size_t how_far)
{
    uint32_t data_sz = 2;
    Status ret = CheckData(ret_short, &data_sz, (int32_t)how_far);

    *ret_short = LITTLE_ENDIAN_SWAP16(*ret_short);

    return ret;

} // Status DataReader::Check16u(uint16_t *ret_short, size_t how_far)

inline
Status DataReader::Check32u(uint32_t *ret_long, size_t how_far)
{
    uint32_t data_sz = 4;
    Status ret = CheckData(ret_long, &data_sz, (int32_t)how_far);

    *ret_long = LITTLE_ENDIAN_SWAP32(*ret_long);

    return ret;

} // Status DataReader::Check32u(uint32_t *ret_long, size_t how_far)

inline
Status DataReader::Get64uNoSwap(unsigned long long *data)
{
    uint32_t data_sz = 8;
    Status ret = GetData(data, &data_sz);

    *data = BIG_ENDIAN_SWAP64(*data);

    return ret;

} // Status DataReader::Get64uNoSwap(unsigned long long *data)

inline
Status DataReader::Get64uSwap(unsigned long long *data)
{
    uint32_t data_sz = 8;
    Status ret = GetData(data, &data_sz);

    *data = LITTLE_ENDIAN_SWAP64(*data);

    return ret;

} // Status DataReader::Get64uSwap(unsigned long long *data)

} // namespace UMC

#endif /* __UIMC_DATA_READER_H__ */
