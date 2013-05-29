/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_pipeline_defs.h"
#include "mfx_directory_reader.h"
#include "ippdefs.h"

#if defined(_WIN32) || defined(_WIN64)

// returns vector of filenames in specified directory
mfxStatus ListFiles(const vm_char *directory, std::vector<tstring> &out)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind;

    hFind = FindFirstFile(directory, &FindFileData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    else
    {
        out.clear();
        do
        {
            // do not list directories
            if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                out.push_back(tstring(FindFileData.cFileName));
        }
        while(FindNextFile(hFind, &FindFileData));

        FindClose(hFind);
    }

    return MFX_ERR_NONE;
}

DirectoryBitstreamReader::DirectoryBitstreamReader(IBitstreamReader *pReader)
: base(pReader)
{
}

DirectoryBitstreamReader::~DirectoryBitstreamReader()
{
    Close();
}

void DirectoryBitstreamReader::Close()
{
    m_Directory.clear();
    m_Files.clear();
    m_pCurrentFile = m_Files.end();

    base::Close();
}

inline bool isPathDelimiter(vm_char c)
{
    return c == '/' || c == '\\';
}

mfxStatus DirectoryBitstreamReader::Init(const vm_char *strFileName)
{
    vm_char l_directory[MAX_PATH];
    vm_string_strcpy_s<MAX_PATH>(l_directory, strFileName);

    size_t len = vm_string_strlen(l_directory);

    if (isPathDelimiter(l_directory[len-1]))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_STS(ListFiles(l_directory, m_Files));

    if (m_Files.empty())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // find last delimiter
    int idx = (int)len - 1;
    while(!isPathDelimiter(l_directory[idx]) && (idx >= 0))
        --idx;
    if (idx < 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // truncate to last delimiter
    l_directory[idx + 1] = 0;
    // cache directory name
    m_Directory = tstring(l_directory);

    m_pCurrentFile = m_Files.begin();

    return base::Init((m_Directory + *m_pCurrentFile).c_str());
}

mfxStatus DirectoryBitstreamReader::ReadNextFrame(mfxBitstream2 &bs)
{
    mfxStatus sts = base::ReadNextFrame(bs);

    // if end of current file reached
    if (MFX_ERR_UNKNOWN == sts)
    {
        tstring filename;
        // if we there are more files in the directory
        if (++m_pCurrentFile != m_Files.end())
        {
            MFX_CHECK_STS(sts = base::Init((m_Directory + *m_pCurrentFile).c_str()));

            sts = base::ReadNextFrame(bs);
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    return sts;
}

#endif //#if defined(_WIN32) || defined(_WIN64)
