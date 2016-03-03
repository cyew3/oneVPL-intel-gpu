/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
#include "xerces_entry.h"

#ifdef XERCES_XML

#include "outline.h"
#include "xerces_common.h"
#include <stdarg.h>

XML_Entry_Read::XML_Entry_Read(DOMElement * element)
    : m_Element(element)
{
}

XML_Entry_Read::~XML_Entry_Read()
{
}

vm_char * XML_Entry_Read::GetAttribute(const vm_char * name) const
{
    return transcodeFromXML<vm_char>(GetXMLAttribute(name));
}

bool XML_Entry_Read::IsAttributeExist(const vm_char * tag)
{
    return GetXMLAttribute(tag) != 0;
}

void XML_Entry_Read::GetAttributeFormat(const vm_char * name, const vm_char * format, ...) const
{
    vm_char * str = transcodeFromXML<vm_char>(GetXMLAttribute(name));

    if (!str)
        throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("XML: can't find attribute value"));

    va_list(arglist);
    va_start(arglist, format);

    vm_string_vsscanf(str, format, arglist);
    va_end(arglist);
}

void XML_Entry_Read::GetValueFormat(const vm_char * tag, const vm_char * format, ...) const
{
    vm_char * str = transcodeFromXML<vm_char>(GetXMLValueByTag(tag));

    if (!str)
        throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("XML: can't find value"));

    va_list(arglist);
    va_start(arglist, format);

    vm_string_vsscanf(str, format, arglist);
    va_end(arglist);
}

vm_char * XML_Entry_Read::GetValueByTag(vm_char * tagName) const
{
    return transcodeFromXML<vm_char>(GetXMLValueByTag(tagName));
}

Ipp32s XML_Entry_Read::GetNumberValueByTag(vm_char * tagName) const
{
    return XMLString::parseInt(GetXMLValueByTag(tagName));
}

Ipp32u XML_Entry_Read::GetUNumberValueByTag(vm_char * tagName) const
{
    vm_char *str = transcodeFromXML<vm_char>(GetXMLValueByTag(tagName));
    if (!str)
        throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("XML: can't find value"));

    Ipp32u value;
    if (vm_string_sscanf(str, VM_STRING("%x"), &value) != 1)
        return Ipp32u(-1);

    return value;
}

const XMLCh * XML_Entry_Read::GetXMLAttribute(const vm_char * name) const
{
    return m_Element->getAttribute(transcodeToXML(name));
}

const XMLCh * XML_Entry_Read::GetXMLValueByTag(const vm_char * tagName) const
{
    DOMNodeList * list = m_Element->getElementsByTagName(transcodeToXML(tagName));
    DOMNode * node = list->item(0);

    if (!node)
        return 0;

    DOMNode *text = node->getFirstChild();

    if (!text)
        return 0;

    return text->getNodeValue();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
XML_Entry_Write::XML_Entry_Write(xercesc::DOMDocument* doc, const vm_char * name)
    : m_Element(0)
    , m_doc(doc)
{
    m_Element = m_doc->createElement(transcodeToXML(name));
}

XML_Entry_Write::~XML_Entry_Write()
{
}

void XML_Entry_Write::CreateAttribute(const vm_char * name)
{
    DOMAttr *attrID = m_doc->createAttribute(transcodeToXML(name));
    m_Element->setAttributeNode(attrID);
}

void XML_Entry_Write::SetChildNodeValue(const vm_char * tag, const vm_char * name)
{
    DOMNodeList * list = m_Element->getElementsByTagName(transcodeToXML(tag));
    DOMNode * node = list->item(0);

    if (!node)
        return;

    DOMNode *text = node->getFirstChild();

    if (!text)
        return;

    text->setNodeValue(transcodeToXML(name));
}

void XML_Entry_Write::SetChildNodeFormatValue(const vm_char * tag, const vm_char * format, ...)
{
    va_list(arglist);
    va_start(arglist, format);

    vm_char value[256];
    vm_string_vsprintf(value, format, arglist);
    va_end(arglist);
    SetChildNodeValue(tag, value);
}

void XML_Entry_Write::SetAttribute(const vm_char * tag, const vm_char * value)
{
    m_Element->setAttribute(transcodeToXML(tag), transcodeToXML(value));
}

void XML_Entry_Write::SetAttributeFormat(const vm_char * tag, const vm_char * format, ...)
{
    va_list(arglist);
    va_start(arglist, format);

    vm_char value[256];
    vm_string_vsprintf(value, format, arglist);
    va_end(arglist);
    SetAttribute(tag, value);
}

void XML_Entry_Write::AppendChild(const vm_char * tag)
{
    DOMElement * el = m_doc->createElement(transcodeToXML(tag));
    m_Element->appendChild(el);

    DOMText * text = m_doc->createTextNode(transcodeToXML(VM_STRING("")));
    el->appendChild(text);
}

DOMElement * XML_Entry_Write::GetElement()
{
    return m_Element;
}

#endif // XERCES_XML
