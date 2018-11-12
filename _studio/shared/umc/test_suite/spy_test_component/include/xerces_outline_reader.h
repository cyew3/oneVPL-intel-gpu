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

#ifndef __XERCES_OUTLINE_READER_H__
#define __XERCES_OUTLINE_READER_H__

#include "umc_structures.h"
#include "xerces_common.h"

#include "test_component.h"

#include "outline.h"
#include <map>

#ifdef XERCES_XML


class XML_Entry_Read;
// Xerces Video OutlineReader class
class XercesOutlineReader : public OutlineReader
{
public:
    XercesOutlineReader();
    virtual ~XercesOutlineReader();

    virtual void Init(const vm_char *filename);
    virtual void Close();

    virtual Entry_Read * GetEntry(Ipp32s seq_index, Ipp32s index);
    virtual Entry_Read * GetSequenceEntry(Ipp32s index);


    virtual ListOfEntries FindEntry(const TestVideoData & tstData, Ipp32u mode);

protected:

    DOMImplementation *m_pImpl;
    xercesc::DOMDocument *m_pOutlineDoc;

    XMLFormatTarget *m_pOutlineFile;
    DOMLSParser *m_pParser;

    typedef std::vector<XML_Entry_Read>   EntriesArray;
    typedef std::map<Ipp32s, EntriesArray > CacheMap;
    CacheMap m_cacheMap;
};


#endif // XERCES_XML

#endif //__XERCES_OUTLINE_READER_H__
