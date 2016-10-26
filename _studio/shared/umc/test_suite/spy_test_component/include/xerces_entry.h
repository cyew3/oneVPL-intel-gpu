//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

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
