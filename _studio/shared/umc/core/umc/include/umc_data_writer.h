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

#ifndef __UMC_DATA_WRITER_H__
#define __UMC_DATA_WRITER_H__

/*
//  Class:       DataWriter
//
//  Notes:       Base abstract class of data writer. Class describes
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

class DataWriterParams
{
    DYNAMIC_CAST_DECL_BASE(DataWriterParams)

public:
    // Default constructor
    DataWriterParams(void){}
    // Destructor
    virtual ~DataWriterParams(void){}
};

class DataWriter
{
    DYNAMIC_CAST_DECL_BASE(DataWriter)
public:
    // Default constructor
    DataWriter(void){}
    // Destructor
    virtual ~DataWriter(void){}

    // Initialization abstract destination media
    virtual Status Init(DataWriterParams *InitParams) = 0;

    // Close destination media
    virtual Status Close(void) = 0;

    // Reset all internal parameters to start writing from begin
    virtual Status Reset(void) = 0;

    // Write data to output stream
    virtual Status PutData(void *data, int32_t &nsize) = 0;

    // Set current position in destination media
    virtual Status SetPosition(uint32_t /* nPosLow */,
                               uint32_t * /* pnPosHigh */,
                               uint32_t /* nMethod */)
    {   return UMC_ERR_FAILED;    }

    // Get current position in destination media
    virtual Status GetPosition(uint32_t * /* pnPosLow */, uint32_t * /* pnPosHigh */)
    {   return UMC_ERR_FAILED;    }
};

} // namespace UMC

#endif /* __UMC_DATA_WRITER_H__ */
