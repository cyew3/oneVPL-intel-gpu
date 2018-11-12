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
