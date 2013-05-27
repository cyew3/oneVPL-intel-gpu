/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2009-2012 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_ibitstream_reader.h"

class DirectoryBitstreamReader
    : public InterfaceProxy<IBitstreamReader>
{
    typedef InterfaceProxy<IBitstreamReader> base;
public:
    DirectoryBitstreamReader(IBitstreamReader *pReader);
    virtual ~DirectoryBitstreamReader();

    virtual void      Close();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &pBS);

protected:
    std::vector<tstring> m_Files;
    std::vector<tstring>::const_iterator m_pCurrentFile;
    tstring m_Directory;
};
