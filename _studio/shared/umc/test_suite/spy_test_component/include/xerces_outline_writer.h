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
