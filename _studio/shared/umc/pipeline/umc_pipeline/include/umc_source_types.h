//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

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
