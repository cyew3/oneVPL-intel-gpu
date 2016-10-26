//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __XERCES_XML_H__
#define __XERCES_XML_H__

#include "umc_video_decoder.h"
#include "test_component.h"
#include "xerces_common.h"

#ifdef XERCES_XML

class XML_Entry_Write;

typedef struct sBaseXMLObjects
{
    DOMImplementation *pImpl;
    xercesc::DOMDocument *pOutlineDoc;

    XMLFormatTarget *pOutlineFile;
    DOMLSOutput *destination;
    DOMLSSerializer *pSerializer;

} BaseXMLObjects;

// Xerces Video OutlineWriter class
class XercesVideoOutlineWriter : public VideoOutlineWriter
{
public:

    XercesVideoOutlineWriter();
    virtual ~XercesVideoOutlineWriter();
    virtual void Init(const vm_char *fileName);
    virtual void Close();
    virtual void WriteFrame(TestVideoData *pData);
    virtual void WriteSequence(Ipp32s seq_id, UMC::BaseCodecParams *info);

    void SetPrettyPrint(bool pretty_print);

protected:
    BaseXMLObjects *m_pBXMLObj;

    void InitDOMFrame(xercesc::DOMDocument* &doc);
    void FillFrame(TestVideoData *pData);
    void FillSequence(Ipp32s seq_id, UMC::VideoDecoderParams *info);

    XML_Entry_Write * m_frameElement;
    XML_Entry_Write * m_sequenceElement;

    bool isSequenceWasWritten;
};


#endif // #ifdef XERCES_XML

#endif // #ifndef __XERCES_XML_H__
