/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_pipeline_defs.h"
#include <iostream>

#define FAIL_ON_ERROR(STS)\
if (STS != ERROR_SUCCESS)\
{\
    PipelineTrace((VM_STRING("ERROR: failed to set or restore registry values\n")));\
}\

#ifdef UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

struct RegValue
{
    HKEY    rootKey;
    tstring subKey;
    tstring valueName;
    bool    existedBefore;
    mfxU32  initialValueData;
    mfxU32  newValueData;
};

class tsRegistryEditor
{
    std::vector<RegValue> m_regValues;
public:
    void SetRegKey(
        HKEY rootKey,
        const tstring& subKey,
        const tstring& valueName,
        mfxU32 valueData,
        bool forceZero = false);
    void RestoreRegKeys();
    ~tsRegistryEditor();
};

void tsRegistryEditor::SetRegKey(
    HKEY rootKey,
    const tstring& subKey,
    const tstring& valueName,
    mfxU32 valueData,
    bool forceZero)
{
    HKEY hkey;
    HKEY hTempKey;

    RegValue value = {};
    value.rootKey = rootKey;
    value.subKey = subKey;
    value.valueName = valueName;
    value.existedBefore = true;

    DWORD size = sizeof(DWORD);
    long sts = RegGetValue(rootKey, subKey.c_str(), valueName.c_str(), RRF_RT_REG_DWORD, NULL, (PVOID)&value.initialValueData, &size);
    if (sts == ERROR_FILE_NOT_FOUND)
    {
        value.existedBefore = false;
        value.initialValueData = 0;
    }
    else FAIL_ON_ERROR(sts);

    bool needToSetValue = (valueData != value.initialValueData || value.existedBefore == false && valueData == 0 && forceZero == true);

    if (needToSetValue)
    {
        sts = RegCreateKey(rootKey, subKey.c_str(), &hTempKey);
        FAIL_ON_ERROR(sts);
        sts = RegSetValueEx(hTempKey, valueName.c_str(), 0, REG_DWORD, (LPBYTE)&valueData, sizeof(DWORD));
        FAIL_ON_ERROR(sts);
        value.newValueData = valueData;
        m_regValues.push_back(value);
    }

    sts = RegCloseKey(rootKey);
    FAIL_ON_ERROR(sts);
}

void tsRegistryEditor::RestoreRegKeys()
{
    for (std::vector<RegValue>::iterator i = m_regValues.begin(); i != m_regValues.end(); i++)
    {
        HKEY hkey;
        long sts = RegOpenKeyEx(i->rootKey, i->subKey.c_str(), 0, KEY_ALL_ACCESS, &hkey);
        FAIL_ON_ERROR(sts);

        if (i->existedBefore == false)
        {
            sts = RegDeleteValue(hkey, i->valueName.c_str());
        }
        else
        {
            sts = RegSetValueEx(hkey, i->valueName.c_str(), 0, REG_DWORD, (LPBYTE)&i->initialValueData, sizeof(DWORD));
        }
        FAIL_ON_ERROR(sts);

        sts = RegCloseKey(hkey);
        FAIL_ON_ERROR(sts);
    }

    m_regValues.clear();
}

tsRegistryEditor::~tsRegistryEditor()
{
    RestoreRegKeys();
}

#endif // #if defined(_WIN32) || defined(_WIN64)