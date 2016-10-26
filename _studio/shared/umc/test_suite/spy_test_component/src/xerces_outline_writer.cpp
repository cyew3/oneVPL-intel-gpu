//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "xerces_outline_writer.h"
#include "outline_writer_defs.h"
#include "umc_structures.h"
#include "umc_video_decoder.h"

#include "umc_h264_dec.h"

#ifdef XERCES_XML

#include "xerces_entry.h"

XercesVideoOutlineWriter::XercesVideoOutlineWriter()
    : m_pBXMLObj(0)
    , m_frameElement(0)
    , m_sequenceElement(0)
    , isSequenceWasWritten(false)
{
    m_pBXMLObj = NULL;
    XMLPlatformUtils::Initialize();
}

XercesVideoOutlineWriter::~XercesVideoOutlineWriter()
{
    Close();
}

void XercesVideoOutlineWriter::Init(const vm_char *fileName)
{
    m_pBXMLObj = new BaseXMLObjects;

    m_pBXMLObj->pImpl = DOMImplementationRegistry::getDOMImplementation(transcodeToXML("LS"));
    m_pBXMLObj->pSerializer = ((DOMImplementationLS*)(m_pBXMLObj->pImpl))->createLSSerializer();

    m_pBXMLObj->pOutlineDoc = m_pBXMLObj->pImpl->createDocument(0, transcodeToXML("outline"), 0);

    m_pBXMLObj->pOutlineFile = new LocalFileFormatTarget(transcodeToXML(fileName));
    const XMLByte strDoctype[] =
        "<!DOCTYPE outline [<!ATTLIST f id ID #REQUIRED>]>\n";
    m_pBXMLObj->pOutlineFile->writeChars(strDoctype, sizeof(strDoctype) - 1, 0);
    const XMLByte str_outline[] = "<outline>";
    m_pBXMLObj->pOutlineFile->writeChars(str_outline, sizeof(str_outline) - 1, 0);

    InitDOMFrame(m_pBXMLObj->pOutlineDoc);

    m_pBXMLObj->destination = m_pBXMLObj->pImpl->createLSOutput();
    m_pBXMLObj->destination->setByteStream(m_pBXMLObj->pOutlineFile);

    SetPrettyPrint(true);
}

void XercesVideoOutlineWriter::Close()
{
    if (m_pBXMLObj)
    {
        // writing closing outline tag to an xml file
        const XMLByte str_outline[] = "\n</outline>";
        m_pBXMLObj->pOutlineFile->writeChars(str_outline, sizeof(str_outline), 0);
        delete m_pBXMLObj->pOutlineFile;
        m_pBXMLObj->pOutlineDoc->release();
        m_pBXMLObj->pSerializer->release();
        delete m_pBXMLObj;
        m_pBXMLObj = 0;
    }
}

void XercesVideoOutlineWriter::WriteFrame(TestVideoData *pData)
{
    FillFrame(pData);
    m_pBXMLObj->pSerializer->write(m_frameElement->GetElement(), m_pBXMLObj->destination);
}

void XercesVideoOutlineWriter::SetPrettyPrint(bool pretty_print)
{
    DOMConfiguration *config = m_pBXMLObj->pSerializer->getDomConfig();

    if (config)
        config->setParameter(transcodeToXML("format-pretty-print"), pretty_print);
}

void XercesVideoOutlineWriter::InitDOMFrame(xercesc::DOMDocument* &doc)
{
    m_frameElement = new XML_Entry_Write(doc, FRAME);

    m_frameElement->CreateAttribute(ID);
    m_frameElement->CreateAttribute(FRAME_TYPE);
    m_frameElement->CreateAttribute(PICTURE_STRUCTURE);
    m_frameElement->CreateAttribute(TIME_STAMP);
    m_frameElement->CreateAttribute(IS_INVALID);
    m_frameElement->CreateAttribute(CRC32);

    m_frameElement->CreateAttribute(MFX_PICTURE_STRUCTURE);
    m_frameElement->CreateAttribute(MFX_DATA_FLAG);

    m_frameElement->CreateAttribute(MFX_VIEW_ID);
    m_frameElement->CreateAttribute(MFX_TEMPORAL_ID);
    m_frameElement->CreateAttribute(MFX_PRIORITY_ID);

    /*if (0)
    {
        m_frameElement->AppendChild(VM_STRING("alpha_crc32"));
    }*/

    m_sequenceElement = new XML_Entry_Write(doc, SEQ_SEQUENCE);

    m_sequenceElement->CreateAttribute(ID);
    m_sequenceElement->AppendChild(SEQ_COLOR_FORMAT);
    m_sequenceElement->AppendChild(SEQ_FRAME_WIDTH);
    m_sequenceElement->AppendChild(SEQ_FRAME_HEIGHT);

    m_sequenceElement->AppendChild(SEQ_FRAME_CROP_WIDTH);
    m_sequenceElement->AppendChild(SEQ_FRAME_CROP_HEIGHT);
    m_sequenceElement->AppendChild(SEQ_FRAME_CROP_X);
    m_sequenceElement->AppendChild(SEQ_FRAME_CROP_Y);

    m_sequenceElement->AppendChild(SEQ_HORZ_ASPECT);
    m_sequenceElement->AppendChild(SEQ_VERT_ASPECT);

    m_sequenceElement->AppendChild(SEQ_PROFILE);
    m_sequenceElement->AppendChild(SEQ_LEVEL);

    m_sequenceElement->AppendChild(SEQ_FRAME_RATE);
    m_sequenceElement->AppendChild(SEQ_BITDEPTH_LUMA);
    m_sequenceElement->AppendChild(SEQ_BITDEPTH_CHROMA);
    m_sequenceElement->AppendChild(SEQ_BITDEPTH_ALPHA);
    m_sequenceElement->AppendChild(SEQ_ALPHA_CHANNEL_PRESENT);
}

void XercesVideoOutlineWriter::WriteSequence(Ipp32s seq_id, UMC::BaseCodecParams *in)
{
    UMC::VideoDecoderParams *info = DynamicCast<UMC::VideoDecoderParams> (in);
    if (!info)
        throw OutlineException(UMC::UMC_ERR_INIT, VM_STRING("XML writer: invalid parameter of WriteSequence: object should derived from VideoDecoderParams"));

    FillSequence(seq_id, info);
    m_pBXMLObj->pSerializer->write(m_sequenceElement->GetElement(), m_pBXMLObj->destination);
}

void XercesVideoOutlineWriter::FillSequence(Ipp32s seq_id, UMC::VideoDecoderParams *info)
{
    UMC::H264VideoDecoderParams *in = DynamicCast<UMC::H264VideoDecoderParams>(info);

    m_sequenceElement->SetAttributeFormat(ID, VM_STRING("%d"), seq_id);
    m_sequenceElement->SetChildNodeFormatValue(SEQ_COLOR_FORMAT, VM_STRING("%d"), info->info.color_format);

    if (in)
    {
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_WIDTH, VM_STRING("%d"), in->m_fullSize.width);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_HEIGHT, VM_STRING("%d"), in->m_fullSize.height);

        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_WIDTH, VM_STRING("%d"), info->info.clip_info.width);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_HEIGHT, VM_STRING("%d"), info->info.clip_info.height);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_X, VM_STRING("%d"), in->m_cropArea.left);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_Y, VM_STRING("%d"), in->m_cropArea.top);
    }
    else
    {
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_WIDTH, VM_STRING("%d"), info->info.clip_info.width);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_HEIGHT, VM_STRING("%d"), info->info.clip_info.height);

        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_WIDTH, VM_STRING("%d"), info->info.clip_info.width);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_HEIGHT, VM_STRING("%d"), info->info.clip_info.height);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_X, VM_STRING("%d"), 0);
        m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_CROP_Y, VM_STRING("%d"), 0);
    }

    m_sequenceElement->SetChildNodeFormatValue(SEQ_HORZ_ASPECT, VM_STRING("%d"), info->info.aspect_ratio_width);
    m_sequenceElement->SetChildNodeFormatValue(SEQ_VERT_ASPECT, VM_STRING("%d"), info->info.aspect_ratio_height);

    m_sequenceElement->SetChildNodeFormatValue(SEQ_PROFILE, VM_STRING("%d"), info->profile);
    m_sequenceElement->SetChildNodeFormatValue(SEQ_LEVEL, VM_STRING("%d"), info->level);
    m_sequenceElement->SetChildNodeFormatValue(SEQ_FRAME_RATE, VM_STRING("%d"), (Ipp32s)(info->info.framerate * 100));
    m_sequenceElement->SetChildNodeFormatValue(SEQ_BITDEPTH_LUMA, VM_STRING("%d"), 8);
    m_sequenceElement->SetChildNodeFormatValue(SEQ_BITDEPTH_CHROMA, VM_STRING("%d"), 8);
    //m_sequenceElement->SetChildNodeFormatValue(SEQ_BITDEPTH_ALPHA, VM_STRING("%d"), horzAspect);
    //m_sequenceElement->SetChildNodeFormatValue(SEQ_ALPHA_CHANNEL_PRESENT, VM_STRING("%d"), horzAspect);
}

void XercesVideoOutlineWriter::FillFrame(TestVideoData *pData)
{
    m_frameElement->SetAttributeFormat(ID, VM_STRING("%d_%d"), pData->GetSequenceNumber(), pData->GetFrameNumber());
    m_frameElement->SetAttributeFormat(FRAME_TYPE, VM_STRING("%d"), pData->GetFrameType());
    m_frameElement->SetAttributeFormat(PICTURE_STRUCTURE, VM_STRING("%d"), pData->GetPictureStructure());

    m_frameElement->SetAttributeFormat(TIME_STAMP, VM_STRING("%llu"), pData->GetIntTime());

    m_frameElement->SetAttributeFormat(IS_INVALID, VM_STRING("%d"), pData->GetInvalid());

    m_frameElement->SetAttributeFormat(CRC32, VM_STRING("%08X"), pData->GetCRC32());

    if (pData->m_is_mfx)
    {
        m_frameElement->SetAttributeFormat(MFX_DATA_FLAG, VM_STRING("%d"), pData->m_mfx_dataFlag);
        m_frameElement->SetAttributeFormat(MFX_PICTURE_STRUCTURE, VM_STRING("%d"), pData->m_mfx_picStruct);

        m_frameElement->SetAttributeFormat(MFX_VIEW_ID, VM_STRING("%d"), pData->m_mfx_viewId);
        m_frameElement->SetAttributeFormat(MFX_TEMPORAL_ID, VM_STRING("%d"), pData->m_mfx_temporalId);
        m_frameElement->SetAttributeFormat(MFX_PRIORITY_ID, VM_STRING("%d"), pData->m_mfx_priorityId);
    }
}


#endif // XERCES_XML
