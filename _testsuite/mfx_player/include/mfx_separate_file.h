/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"

class SeparateFilesWriter
    : public InterfaceProxy<IFile>
{
    IMPLEMENT_CLONE(SeparateFilesWriter);

public :
    SeparateFilesWriter(std::unique_ptr<IFile> &&ptarget)
        : InterfaceProxy<IFile>(std::move(ptarget))
        , m_nTimesReopened(0)
    {
    }
    //if file already opened it will create new with different name
    virtual mfxStatus Reopen()
    {
        if (!isOpen())
            return MFX_ERR_NONE;

        MFX_CHECK_STS(Close());

        //TODO: possible external file modificators required
        if (m_file_name.empty() && m_file_ext.empty() && !m_path.empty())
        {
            //if file name provided lets grab extension

            size_t dotPos = m_path.find_last_of(VM_STRING('.'));
            m_file_name = m_path.substr(0, dotPos);
            m_file_ext = m_path.substr(dotPos + 1);
        }

        tstringstream stream;
        stream<< m_file_name << VM_STRING("_") << ++m_nTimesReopened << VM_STRING(".") << m_file_ext;

        //calling base open, to not clear reopened counter
        mfxStatus sts = InterfaceProxy<IFile>::Open(stream.str(), m_access);
        PipelineTrace((VM_STRING("output file opened : %s\n"), stream.str().c_str()));

        return sts;
    }

    virtual mfxStatus Open(const tstring & path, const tstring& access)
    {
        m_path           = path;
        m_access         = access;
        m_nTimesReopened = 0;
        m_file_name.clear();
        m_file_ext.clear();

        return InterfaceProxy<IFile>::Open(path, access);
    }

protected:
    SeparateFilesWriter(const SeparateFilesWriter& that)
        : InterfaceProxy<IFile>(that)
        , m_nTimesReopened()
    {
    }
    tstring m_file_name;
    tstring m_file_ext;
    tstring m_path;
    tstring m_access;
    mfxU32  m_nTimesReopened;
};
