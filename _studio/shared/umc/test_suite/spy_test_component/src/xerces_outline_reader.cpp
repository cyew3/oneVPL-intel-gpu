//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "xerces_outline_reader.h"
#include "umc_structures.h"
#include "outline_writer_defs.h"
#include "umc_h264_dec.h"

#ifdef XERCES_XML

#include "xerces_entry.h"

void OutlineReader::ReadEntryStruct(Entry_Read * entry, TestVideoData * data)
{
    Ipp32s temp, temp1;
    vm_string_sscanf(entry->GetAttribute(ID), VM_STRING("%d_%d"), &temp, &temp1);
    //entry->GetAttributeFormat(ID, VM_STRING("%d_%d"), &temp, &temp1);
    data->SetSequenceNumber(temp);
    data->SetFrameNumber(temp1);

    entry->GetAttributeFormat(FRAME_TYPE, VM_STRING("%d"), &temp);
    data->SetFrameType((UMC::FrameType)temp);

    entry->GetAttributeFormat(PICTURE_STRUCTURE, VM_STRING("%d"), &temp);
    data->SetPictureStructure((UMC::PictureStructure)temp);

    Ipp64u ts;

    entry->GetAttributeFormat(TIME_STAMP, VM_STRING("%llu"), &ts);
    data->SetIntTime(ts);
    data->SetTime(GetFloatTimeStamp(ts));

    entry->GetAttributeFormat(IS_INVALID, VM_STRING("%d"), &temp);
    data->SetInvalid(temp);

    entry->GetAttributeFormat(CRC32, VM_STRING("%08X"), &temp);
    data->SetCRC32(temp);

    if (entry->IsAttributeExist(MFX_DATA_FLAG))
    {
        data->m_is_mfx = true;
        temp = 0;

        entry->GetAttributeFormat(MFX_DATA_FLAG, VM_STRING("%d"), &temp);
        data->m_mfx_dataFlag = temp;

        entry->GetAttributeFormat(MFX_PICTURE_STRUCTURE, VM_STRING("%d"), &temp);
        data->m_mfx_picStruct = temp;

        entry->GetAttributeFormat(MFX_VIEW_ID, VM_STRING("%d"), &temp);
        data->m_mfx_viewId = temp;

        entry->GetAttributeFormat(MFX_TEMPORAL_ID, VM_STRING("%d"), &temp);
        data->m_mfx_temporalId = temp;

        entry->GetAttributeFormat(MFX_PRIORITY_ID, VM_STRING("%d"), &temp);
        data->m_mfx_priorityId = temp;
    }
}

void OutlineReader::ReadSequenceStruct(Entry_Read * entry, UMC::VideoDecoderParams *info)
{
    Ipp32s temp;

    entry->GetAttributeFormat(ID, VM_STRING("%d"), &temp);

    entry->GetValueFormat(SEQ_COLOR_FORMAT, VM_STRING("%d"), &temp);
    info->info.color_format = (UMC::ColorFormat) temp;

    UMC::H264VideoDecoderParams *in = DynamicCast<UMC::H264VideoDecoderParams>(info);
    if (in)
    {
        entry->GetValueFormat(SEQ_FRAME_WIDTH, VM_STRING("%d"), &temp);
        in->m_fullSize.width = temp;
        entry->GetValueFormat(SEQ_FRAME_HEIGHT, VM_STRING("%d"), &temp);
        in->m_fullSize.height = temp;

        entry->GetValueFormat(SEQ_FRAME_CROP_WIDTH, VM_STRING("%d"), &temp);
        in->info.clip_info.width = temp;
        entry->GetValueFormat(SEQ_FRAME_CROP_HEIGHT, VM_STRING("%d"), &temp);
        in->info.clip_info.height = temp;

        entry->GetValueFormat(SEQ_FRAME_CROP_X, VM_STRING("%d"), &temp);
        in->m_cropArea.left = (Ipp16s)temp;
        entry->GetValueFormat(SEQ_FRAME_CROP_Y, VM_STRING("%d"), &temp);
        in->m_cropArea.top = (Ipp16s)temp;

        in->m_cropArea.bottom = (Ipp16s)(in->m_cropArea.top + in->info.clip_info.height);
        in->m_cropArea.right  = (Ipp16s)(in->m_cropArea.left + in->info.clip_info.width);
    }
    else
    {
        entry->GetValueFormat(SEQ_FRAME_WIDTH, VM_STRING("%d"), &temp);
        info->info.clip_info.width = temp;
        entry->GetValueFormat(SEQ_FRAME_HEIGHT, VM_STRING("%d"), &temp);
        info->info.clip_info.height = temp;
    }

    entry->GetValueFormat(SEQ_HORZ_ASPECT, VM_STRING("%d"), &temp);
    info->info.aspect_ratio_width = temp;
    entry->GetValueFormat(SEQ_VERT_ASPECT, VM_STRING("%d"), &temp);
    info->info.aspect_ratio_height = temp;

    entry->GetValueFormat(SEQ_PROFILE, VM_STRING("%d"), &temp);
    info->profile = temp;
    info->info.profile = temp;

    entry->GetValueFormat(SEQ_LEVEL, VM_STRING("%d"), &temp);
    info->level = temp;
    info->info.level = temp;

    entry->GetValueFormat(SEQ_FRAME_RATE, VM_STRING("%d"), &temp);
    info->info.framerate = Ipp64f(temp) / 100;

    entry->GetValueFormat(SEQ_BITDEPTH_LUMA, VM_STRING("%d"), &temp);
    entry->GetValueFormat(SEQ_BITDEPTH_CHROMA, VM_STRING("%d"), &temp);
}

XercesOutlineReader::XercesOutlineReader()
    : m_pImpl(0)
    , m_pOutlineDoc(0)
    , m_pOutlineFile(0)
    , m_pParser(0)
{
    XMLPlatformUtils::Initialize();
};

XercesOutlineReader::~XercesOutlineReader()
{
    try
    {
        Close();
        XMLPlatformUtils::Terminate();
    } catch(...)
    {
    }
};

void XercesOutlineReader::Init(const vm_char *filename)
{
    Close();

    XMLCh tempStr[100];

    XMLString::transcode("LS", tempStr, sizeof(tempStr) - 1);
    m_pImpl = DOMImplementationRegistry::getDOMImplementation(tempStr);

    if (!m_pImpl)
        throw OutlineException(UMC::UMC_ERR_INIT, VM_STRING("XML reader: can't initialize DOM implementation"));

    XMLString::transcode("outline", tempStr, sizeof(tempStr) - 1);
    m_pParser = ((DOMImplementationLS *)m_pImpl)->createLSParser
        (DOMImplementationLS::MODE_SYNCHRONOUS, 0);

    if (!m_pParser)
        throw OutlineException(UMC::UMC_ERR_INIT, VM_STRING("XML reader: can't initialize parser"));

    m_pOutlineDoc = m_pParser->parseURI(transcodeToXML(filename));

    if (!m_pOutlineDoc)
        throw OutlineException(UMC::UMC_ERR_INIT, VM_STRING("XML reader: can't create document"));
};

void XercesOutlineReader::Close()
{
    if (m_pOutlineDoc)
    {
        m_pParser->release();
    }
};

Entry_Read * XercesOutlineReader::GetEntry(Ipp32s seq_index, Ipp32s index)
{
    vm_char  idx_str[128];

    vm_string_sprintf(idx_str, VM_STRING("%d_%d"), seq_index, index);

    DOMElement * el = m_pOutlineDoc->getElementById(transcodeToXML(idx_str));

    if (!el)
        return 0;

    XML_Entry_Read * entry = new XML_Entry_Read(el);
    return entry;
}

Entry_Read * XercesOutlineReader::GetSequenceEntry(Ipp32s index)
{
    XMLCh   idx_str[128];

    XMLString::binToText(index, idx_str, sizeof(idx_str) - 1, 10);

    DOMNodeList* list = m_pOutlineDoc->getElementsByTagName(transcodeToXML(SEQ_SEQUENCE));

    if (!list)
        return 0;

    if (index > (Ipp32s)list->getLength() || index < 0)
        return 0;

    DOMElement * node = (DOMElement*)list->item(index);
    if (!node)
        return 0;

    XML_Entry_Read * entry = new XML_Entry_Read(node);
    return entry;
}


std::list<Entry_Read *> XercesOutlineReader::FindEntry(const TestVideoData & tstData, Ipp32u mode)
{
    std::list<Entry_Read *> entries;

    if (mode == FIND_MODE_ID)
    {
        Entry_Read * entry = GetEntry(tstData.GetSequenceNumber(), tstData.GetFrameNumber());
        if (entry)
            entries.push_back(entry);
        return entries;
    }

    CacheMap::iterator sequence_iter = m_cacheMap.find(tstData.GetSequenceNumber());

    if (sequence_iter == m_cacheMap.end())
    {
        vm_char  idx_str[128];

        vm_string_sprintf(idx_str, VM_STRING("%d_"), tstData.GetSequenceNumber());
        size_t idx_len = vm_string_strlen(idx_str);

        DOMNodeList* list = m_pOutlineDoc->getElementsByTagName(transcodeToXML(FRAME));

        EntriesArray elements;

        for (Ipp32u i = 0; i < list->getLength(); i++)
        {
            DOMElement * node = (DOMElement*)list->item(i);

            XML_Entry_Read entry(node);

            if (vm_string_strncmp(idx_str, entry.GetAttribute(ID), idx_len) == 0)
            {
                elements.push_back(entry);
            }
        }

        m_cacheMap[tstData.GetSequenceNumber()] = elements;
        sequence_iter = m_cacheMap.find(tstData.GetSequenceNumber());
    }

    Ipp64u timestamp = tstData.GetIntTime();
    Ipp32u crc32_original = tstData.GetCRC32();

    EntriesArray & all_entries = sequence_iter->second;

    EntriesArray::iterator iter = all_entries.begin();

    for (; iter != all_entries.end(); ++iter)
    {
        XML_Entry_Read & entry = *iter;

        Ipp64u ts;
        entry.GetAttributeFormat(TIME_STAMP, VM_STRING("%I64d"), &ts);

        bool canAdd = false;
        if (mode & FIND_MODE_TIMESTAMP)
        {
            Ipp64u ts;
            entry.GetAttributeFormat(TIME_STAMP, VM_STRING("%llu"), &ts);

            if (ts == timestamp)
                canAdd = true;
        }

        if (mode & FIND_MODE_CRC)
        {
            Ipp32u crc32;
            entry.GetAttributeFormat(CRC32, VM_STRING("%08X"), &crc32);
            if (crc32 == crc32_original)
                canAdd = true;
        }

        if (canAdd)
            entries.push_back(&entry);
    }

    return entries;
}

#endif // XERCES_XML
