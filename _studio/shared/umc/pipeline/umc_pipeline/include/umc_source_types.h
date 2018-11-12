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

#ifndef __UMC_SOURCE_TYPES_H__
#define __UMC_SOURCE_TYPES_H__

#include "vm_types.h"
#include "umc_data_reader.h"

namespace UMC
{

// base splitter source class
class SourceInfo
{
    DYNAMIC_CAST_DECL_BASE(SourceInfo)

public:
    // Default constructor
    SourceInfo();
    // Clone source info item
    virtual SourceInfo *Clone(void) = 0;
    // Load parameter(s)
    virtual bool Load(Ipp8u *lpbBuffer, size_t nBufferSize);
    // Save parameter(s)
    virtual bool Save(Ipp8u *lpbBuffer, size_t &nBufferSize);

    vm_char m_cName[MAXIMUM_PATH];                          // (vm_char) name of media source (file name etc.)
};

// splitter source file param(s)
class SourceInfoFile : public SourceInfo
{
    DYNAMIC_CAST_DECL(SourceInfoFile, SourceInfo)

public:
    // Clone source info item
    virtual SourceInfo *Clone(void);

    UMC::DataReader *m_lpDataReader;                        // (UMC::DataReader *) pointer to data reader
};

// splitter source network param(s)
class SourceInfoNet : public SourceInfo
{
    DYNAMIC_CAST_DECL(SourceInfoNet, SourceInfo)

public:
    // Clone source info item
    virtual SourceInfo *Clone(void);
    // Load parameter(s)
    virtual bool Load(Ipp8u *lpbBuffer, size_t nBufferSize);
    // Save parameter(s)
    virtual bool Save(Ipp8u *lpbBuffer, size_t &nBufferSize);

    Ipp32s m_nPortNumber;                                   // (Ipp32s) port number
};

// splitter source camera param(s)
class SourceInfoWebCam : public SourceInfo
{
    DYNAMIC_CAST_DECL(SourceInfoWebCam, SourceInfo)

public:
    // Clone source info item
    virtual SourceInfo *Clone(void);

     virtual bool Load(Ipp8u *lpbBuffer, size_t nBufferSize);
    // Save parameter(s)
    virtual bool Save(Ipp8u *lpbBuffer, size_t &nBufferSize);

    Ipp32u m_uiResolutionX;
    Ipp32u m_uiResolutionY;
    Ipp32u m_uiBitrate;
    Ipp32f m_fFramerate;
};

// splitter source camera param(s)
class SourceInfoCam : public SourceInfo
{
    DYNAMIC_CAST_DECL(SourceInfoCam, SourceInfo)

public:
    // Clone source info item
    virtual SourceInfo *Clone(void);
};

} //end namespace UMC

#endif //__UMC_SOURCE_TYPES_H__
