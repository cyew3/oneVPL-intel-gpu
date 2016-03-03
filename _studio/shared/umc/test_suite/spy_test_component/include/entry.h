/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __ENTRY_H__
#define __ENTRY_H__

#include "vm_strings.h"

class Entry_Read
{
public:
    Entry_Read()
    {
    }

    virtual ~Entry_Read()
    {
    }

    virtual vm_char * GetValueByTag(vm_char * tagName) const = 0;

    virtual void GetValueFormat(const vm_char * tag, const vm_char * format, ...) const = 0;

    virtual Ipp32s GetNumberValueByTag(vm_char * tagName) const = 0;

    virtual Ipp32u GetUNumberValueByTag(vm_char * tagName) const = 0;

    virtual vm_char * GetAttribute(const vm_char * name) const = 0;

    virtual void GetAttributeFormat(const vm_char * tag, const vm_char * format, ...) const = 0;

    virtual bool IsAttributeExist(const vm_char * tag) = 0;
};

class Entry_Write
{
public:
    Entry_Write()
    {
    }

    virtual ~Entry_Write()
    {
    }

    virtual void CreateAttribute(const vm_char * name) = 0;

    virtual void SetChildNodeValue(const vm_char * tag, const vm_char * name) = 0;

    virtual void SetChildNodeFormatValue(const vm_char * tag, const vm_char * format, ...) = 0;

    virtual void SetAttribute(const vm_char * tag, const vm_char * value) = 0;

    virtual void SetAttributeFormat(const vm_char * tag, const vm_char * format, ...) = 0;

    virtual void AppendChild(const vm_char * tag) = 0;
};

#endif // #ifndef __ENTRY_H__
