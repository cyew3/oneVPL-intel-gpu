//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

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
