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

#ifndef __XERCES_ENTRY_H__
#define __XERCES_ENTRY_H__

#include "entry.h"

#ifdef XERCES_XML

#include "xerces_common.h"

class XML_Entry_Read : public Entry_Read
{
public:
    XML_Entry_Read(DOMElement * element);

    virtual ~XML_Entry_Read();

    virtual vm_char * GetValueByTag(vm_char * tagName) const;

    virtual void GetValueFormat(const vm_char * tag, const vm_char * format, ...) const;

    virtual Ipp32s GetNumberValueByTag(vm_char * tagName) const;

    virtual Ipp32u GetUNumberValueByTag(vm_char * tagName) const;

    virtual vm_char * GetAttribute(const vm_char * name) const;

    virtual void GetAttributeFormat(const vm_char * tag, const vm_char * format, ...) const;

    virtual bool IsAttributeExist(const vm_char * tag);

private:
    DOMElement * m_Element;

    const XMLCh * GetXMLValueByTag(const vm_char * tagName) const;
    const XMLCh * GetXMLAttribute(const vm_char * name) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
class XML_Entry_Write : Entry_Write
{
public:
    XML_Entry_Write(xercesc::DOMDocument* doc, const vm_char * name);

    virtual ~XML_Entry_Write();

    virtual void CreateAttribute(const vm_char * name);

    virtual void SetChildNodeValue(const vm_char * tag, const vm_char * name);

    virtual void SetChildNodeFormatValue(const vm_char * tag, const vm_char * format, ...);

    virtual void SetAttribute(const vm_char * tag, const vm_char * value);

    virtual void SetAttributeFormat(const vm_char * tag, const vm_char * format, ...);

    virtual void AppendChild(const vm_char * tag);

    xercesc::DOMElement * GetElement();

private:
    xercesc::DOMElement * m_Element;
    xercesc::DOMDocument* m_doc;
};

#endif // #ifdef XERCES_XML

#endif // #ifndef __XERCES_ENTRY_H__
