//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"
#include "mfx_mjpeg_encode_interface.h"

using namespace MfxHwMJpegEncode;


mfxStatus MfxHwMJpegEncode::QueryHwCaps(VideoCORE * core, JpegEncCaps & hwCaps)
{
    //Should be replaced with once quering capabs as other encoders do

    // FIXME: remove this when driver starts returning actual encode caps
    hwCaps.MaxPicWidth      = 4096;
    hwCaps.MaxPicHeight     = 4096;

    if (core && core->GetVAType() == MFX_HW_VAAPI && core->GetHWType() < MFX_HW_CHT)
        return MFX_ERR_UNSUPPORTED;

    std::auto_ptr<DriverEncoder> ddi;
    ddi.reset( CreatePlatformMJpegEncoder(core) );
    if (ddi.get() == 0)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, 640, 480, true);
    MFX_CHECK_STS(sts);

    sts = ddi->QueryEncodeCaps(hwCaps);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

bool MfxHwMJpegEncode::IsJpegParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION ||
        id == MFX_EXTBUFF_JPEG_QT ||
        id == MFX_EXTBUFF_JPEG_HUFFMAN /*||
        id == MFX_EXTBUFF_JPEG_PAYLOAD*/;
}

mfxStatus MfxHwMJpegEncode::CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (!IsJpegParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check if buffer presents twice in video param
        if (GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwMJpegEncode::CheckJpegParam(VideoCORE *core, mfxVideoParam & par, JpegEncCaps const & hwCaps)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(hwCaps.Baseline, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.Sequential, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.Huffman, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    MFX_CHECK((par.mfx.Interleaved && hwCaps.Interleaved) || (!par.mfx.Interleaved && hwCaps.NonInterleaved),
        MFX_WRN_PARTIAL_ACCELERATION);

    MFX_CHECK(par.mfx.FrameInfo.Width > 0 && par.mfx.FrameInfo.Height > 0,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    mfxF64 BytesPerPx = 0;
    switch (par.mfx.FrameInfo.FourCC)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
            BytesPerPx = 1.5;
            break;
        case MFX_FOURCC_YUY2:
            BytesPerPx = 2;
            break;
        case MFX_FOURCC_RGB4:
        default:
            BytesPerPx = 4;
    }
    if (core->GetVAType() == MFX_HW_D3D9)
    {
        MFX_CHECK(par.mfx.FrameInfo.Height <= hwCaps.MaxPicWidth/BytesPerPx, MFX_WRN_PARTIAL_ACCELERATION );
    }

    MFX_CHECK(par.mfx.FrameInfo.Width <= (mfxU16)hwCaps.MaxPicWidth && par.mfx.FrameInfo.Height <= (mfxU16)hwCaps.MaxPicHeight,
        MFX_WRN_PARTIAL_ACCELERATION);

    MFX_CHECK(hwCaps.SampleBitDepth == 8, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.MaxNumComponent == 3, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.MaxNumScan >= 1, MFX_WRN_PARTIAL_ACCELERATION);

    mfxStatus sts = CheckExtBufferId(par);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxExtJPEGQuantTables* qt_in  = (mfxExtJPEGQuantTables*)GetExtBuffer( par.ExtParam, par.NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables* ht_in  = (mfxExtJPEGHuffmanTables*)GetExtBuffer( par.ExtParam, par.NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    if (qt_in && qt_in->NumTable > hwCaps.MaxNumQuantTable)
        return MFX_WRN_PARTIAL_ACCELERATION;
    if (ht_in && (ht_in->NumDCTable > hwCaps.MaxNumHuffTable || ht_in->NumACTable > hwCaps.MaxNumHuffTable))
        return MFX_WRN_PARTIAL_ACCELERATION;

    return MFX_ERR_NONE;
}

mfxStatus MfxHwMJpegEncode::FastCopyFrameBufferSys2Vid(
    VideoCORE    * core,
    mfxMemId       vidMemId,
    mfxFrameData & sysSurf,
    mfxFrameInfo & frmInfo
    )
{
    MFX_CHECK_NULL_PTR1(core);
    mfxFrameData vidSurf = { 0 };
    mfxStatus sts = MFX_ERR_NONE;
    vidSurf.MemId = vidMemId;
    
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy input (sys->vid)");
        mfxFrameSurface1 surfSrc = { {0,}, frmInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {0,}, frmInfo, vidSurf };
        sts = core->DoFastCopyWrapper(&surfDst,MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET, &surfSrc,MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY);
        MFX_CHECK_STS(sts);
    }
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus ExecuteBuffers::Init(mfxVideoParam const *par, mfxEncodeCtrl const * ctrl, JpegEncCaps const * hwCaps)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 fourCC = par->mfx.FrameInfo.FourCC;
    mfxU16 chromaFormat = par->mfx.FrameInfo.ChromaFormat;

    mfxExtJPEGQuantTables* jpegQT = NULL;
    mfxExtJPEGHuffmanTables* jpegHT = NULL;
    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam > 0)
    {
        jpegQT = (mfxExtJPEGQuantTables*)   GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_QT );
        jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    }

    mfxExtJPEGQuantTables* jpegQTInitial = (mfxExtJPEGQuantTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables* jpegHTInitial = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    m_payload_base.length = 0;
    m_payload_list.clear();
    if (fourCC == MFX_FOURCC_RGB4 && chromaFormat == MFX_CHROMAFORMAT_YUV444)
    {
        m_app14_data.header    = 0xEEFF;//APP14
        m_app14_data.lenH      = 0;
        m_app14_data.lenL      = 14;
        m_app14_data.s[0]      = 0x41;//"A"
        m_app14_data.s[1]      = 0x64;//"D"
        m_app14_data.s[2]      = 0x6F;//"O"
        m_app14_data.s[3]      = 0x62;//"B"
        m_app14_data.s[4]      = 0x65;//"E"
        m_app14_data.versionH  = 0;
        m_app14_data.versionL  = 0x64;
        m_app14_data.flags0H   = 0;
        m_app14_data.flags0L   = 0;
        m_app14_data.flags1H   = 0;
        m_app14_data.flags1L   = 0;
        m_app14_data.transform = 0; //RGB
        
        mfxU32 payloadSize = 16;
        if (m_payload_base.length + payloadSize > m_payload_base.maxLength)
        {
            mfxU8* data = new mfxU8[m_payload_base.length + payloadSize];
            memcpy_s(data, m_payload_base.length, m_payload_base.data, m_payload_base.length);
            delete[] m_payload_base.data;
            m_payload_base.data = data;
            m_payload_base.maxLength = m_payload_base.length + payloadSize;
        }
        m_payload_list.resize(1);
        m_payload_list.back().data = m_payload_base.data + m_payload_base.length;
        memcpy_s(m_payload_list.back().data, payloadSize, &m_app14_data, payloadSize);
        m_payload_list.back().length = payloadSize;
        m_payload_base.length += m_payload_list.back().length;
    }
    else
    {
        m_app0_data.header      = 0xE0FF;//APP0
        m_app0_data.lenH        = 0;
        m_app0_data.lenL        = 16;
        m_app0_data.s[0]        = 0x4A;//"J"
        m_app0_data.s[1]        = 0x46;//"F"
        m_app0_data.s[2]        = 0x49;//"I"
        m_app0_data.s[3]        = 0x46;//"F"
        m_app0_data.s[4]        = 0;   // 0
        m_app0_data.versionH    = 0x01;
        m_app0_data.versionL    = 0x02;
        m_app0_data.units       = JRU_NONE;
        m_app0_data.xDensity    = 0x0100;//1
        m_app0_data.yDensity    = 0x0100;//1
        m_app0_data.xThumbnails = 0;
        m_app0_data.yThumbnails = 0;
        
        mfxU32 payloadSize = 18;
        if (m_payload_base.length + payloadSize > m_payload_base.maxLength)
        {
            mfxU8* data = new mfxU8[m_payload_base.length + payloadSize];
            memcpy_s(data, m_payload_base.length, m_payload_base.data, m_payload_base.length);
            delete[] m_payload_base.data;
            m_payload_base.data = data;
            m_payload_base.maxLength = m_payload_base.length + payloadSize;
        }
        m_payload_list.resize(1);
        m_payload_list.back().data = m_payload_base.data + m_payload_base.length;
        memcpy_s(m_payload_list.back().data, payloadSize, &m_app0_data, payloadSize);
        m_payload_list.back().length = payloadSize;
        m_payload_base.length += m_payload_list.back().length;
    }

    if (ctrl && ctrl->Payload && ctrl->NumPayload > 0)
    {
        for (mfxU16 i=0; i<ctrl->NumPayload; i++)
        {
            mfxPayload* pExtPayload = ctrl->Payload[i];
            if (pExtPayload)
            {
                if (pExtPayload->Data && (pExtPayload->NumBit>>3) > 0)
                {
                    mfxU32 payloadSize = pExtPayload->NumBit>>3;
                    if (m_payload_base.length + payloadSize > m_payload_base.maxLength)
                    {
                        mfxU8* data = new mfxU8[m_payload_base.length + payloadSize];
                        memcpy_s(data, m_payload_base.length, m_payload_base.data, m_payload_base.length);
                        delete[] m_payload_base.data;
                        m_payload_base.data = data;
                        m_payload_base.maxLength = m_payload_base.length + payloadSize;
                    }
                    m_payload_list.resize(m_payload_list.size()+1);
                    m_payload_list.back().data = m_payload_base.data + m_payload_base.length;
                    memcpy_s(m_payload_list.back().data, payloadSize, pExtPayload->Data, payloadSize);
                    m_payload_list.back().length = payloadSize;
                    m_payload_base.length += m_payload_list.back().length;
                }
                else
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }
            }
        }
    }

#if defined (MFX_VA_WIN)
    // Picture Header
    memset(&m_pps, 0, sizeof(m_pps));
    m_pps.Profile        = 0; // Baseline
    m_pps.Progressive    = 0;
    m_pps.Huffman        = 1;
    m_pps.Interleaved    = (par->mfx.Interleaved != 0);
    m_pps.Differential   = 0;
    m_pps.PicWidth       = (UINT)par->mfx.FrameInfo.CropW;
    m_pps.PicHeight      = (UINT)par->mfx.FrameInfo.CropH;
    m_pps.SampleBitDepth = 8;
    m_pps.ComponentID[0] = 0;
    m_pps.ComponentID[1] = 1;
    m_pps.ComponentID[2] = 2;

    if (!jpegQT && !jpegQTInitial)
        m_pps.Quality = (par->mfx.Quality > 100) ? 100 : par->mfx.Quality;

    if (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_YUV420)
    {
        m_pps.InputSurfaceFormat = 1; // NV12
        m_pps.NumComponent = 3;
    }
    else if (fourCC == MFX_FOURCC_YUY2 && chromaFormat == MFX_CHROMAFORMAT_YUV422H)
    {
        m_pps.InputSurfaceFormat = 3; // YUY2
        m_pps.NumComponent = 3;
    }
    else if (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_YUV400)
    {
        m_pps.InputSurfaceFormat = 4; // Y8
        m_pps.NumComponent = 1;
    }
    else if (fourCC == MFX_FOURCC_RGB4 && chromaFormat == MFX_CHROMAFORMAT_YUV444)
    {
        m_pps.InputSurfaceFormat = 5; // ARGB/ABGR/AYUV
        m_pps.NumComponent = 3;
    }
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // Scan Header
    m_pps.NumScan = 1;
    m_scan_list.resize(1);
    memset(&m_scan_list[0], 0, sizeof(m_scan_list[0]));
    m_scan_list[0].RestartInterval = par->mfx.RestartInterval;
    m_scan_list[0].NumComponent = m_pps.NumComponent;
    m_scan_list[0].ComponentSelector[0] = 0;
    m_scan_list[0].ComponentSelector[1] = 1;
    m_scan_list[0].ComponentSelector[2] = 2;
    m_scan_list[0].FirstDCTCoeff = 0;
    m_scan_list[0].LastDCTCoeff = 0;
    m_scan_list[0].Ah = 0;
    m_scan_list[0].Al = 0;

    // Quanlization tables
    if (jpegQT || jpegQTInitial)
    {
        // External tables
        mfxExtJPEGQuantTables *pExtQuant = jpegQT ? jpegQT : jpegQTInitial;
        MFX_CHECK(pExtQuant->NumTable && (pExtQuant->NumTable <= hwCaps->MaxNumQuantTable), MFX_ERR_UNDEFINED_BEHAVIOR);
        m_pps.NumQuantTable = (UINT)pExtQuant->NumTable;
        m_dqt_list.resize(m_pps.NumQuantTable);
        for (mfxU16 j = 0; j < m_pps.NumQuantTable; j++)
        {
            m_dqt_list[j].TableID   = j;
            m_dqt_list[j].Precision = 0; /* (UINT)pExtQuant->Precision[j];*/
            MFX_INTERNAL_CPY(m_dqt_list[j].Qm, &pExtQuant->Qm[j], 64 * sizeof(USHORT));
        }
        if (m_pps.NumQuantTable == 1)
        {
            m_pps.QuantTableSelector[0] = 0;
            m_pps.QuantTableSelector[1] = 0;
            m_pps.QuantTableSelector[2] = 0;
        }
        else if (m_pps.NumQuantTable == 2)
        {
            m_pps.QuantTableSelector[0] = 0;
            m_pps.QuantTableSelector[1] = 1;
            m_pps.QuantTableSelector[2] = 1;
        }
        else if (m_pps.NumQuantTable == 3)
        {
            m_pps.QuantTableSelector[0] = 0;
            m_pps.QuantTableSelector[1] = 1;
            m_pps.QuantTableSelector[2] = 2;
        }
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    else
    {
        // No external tables - use Quality parameter
        m_pps.NumQuantTable = 0;
        m_dqt_list.resize(m_pps.NumQuantTable);

        if (fourCC == MFX_FOURCC_RGB4)
        {
            m_pps.QuantTableSelector[0] = 0;
            m_pps.QuantTableSelector[1] = 0;
            m_pps.QuantTableSelector[2] = 0;
        }
        else
        {
            m_pps.QuantTableSelector[0] = 0;
            m_pps.QuantTableSelector[1] = 1;
            m_pps.QuantTableSelector[2] = 1;
        }
    }

    // Huffman tables
    if (jpegHT || jpegHTInitial)
    {
        // External tables
        mfxExtJPEGHuffmanTables *pExtHuffman = jpegHT ? jpegHT : jpegHTInitial;
        MFX_CHECK(pExtHuffman->NumDCTable &&
                  pExtHuffman->NumACTable &&
                 (pExtHuffman->NumDCTable <= hwCaps->MaxNumHuffTable) &&
                 (pExtHuffman->NumACTable <= hwCaps->MaxNumHuffTable), MFX_ERR_UNDEFINED_BEHAVIOR);
        m_pps.NumCodingTable = (UINT)(pExtHuffman->NumDCTable + pExtHuffman->NumACTable);
        m_dht_list.resize(m_pps.NumCodingTable);
        for (mfxU16 j = 0; j < pExtHuffman->NumDCTable; j++)
        {
            m_dht_list[j].TableClass = 0;
            m_dht_list[j].TableID    = j;
            MFX_INTERNAL_CPY(m_dht_list[j].BITS, pExtHuffman->DCTables[j].Bits, 16 * sizeof(UCHAR));
            MFX_INTERNAL_CPY(m_dht_list[j].HUFFVAL, pExtHuffman->DCTables[j].Values, 12 * sizeof(UCHAR));
        }
        for (mfxU16 j = 0; j < pExtHuffman->NumACTable; j++)
        {
            m_dht_list[pExtHuffman->NumDCTable + j].TableClass = 1;
            m_dht_list[pExtHuffman->NumDCTable + j].TableID    = j;
            MFX_INTERNAL_CPY(m_dht_list[pExtHuffman->NumDCTable + j].BITS, pExtHuffman->ACTables[j].Bits, 16 * sizeof(UCHAR));
            MFX_INTERNAL_CPY(m_dht_list[pExtHuffman->NumDCTable + j].HUFFVAL, pExtHuffman->ACTables[j].Values, 162 * sizeof(UCHAR));
        }
    }
    else
    {
        // Internal tables
        if (hwCaps->MaxNumHuffTable == 0)
        {
            m_pps.NumCodingTable = 0;
            m_dht_list.resize(m_pps.NumCodingTable);
        }
        else if (hwCaps->MaxNumHuffTable == 1 || fourCC == MFX_FOURCC_RGB4)
        {
            m_pps.NumCodingTable = 2;
            m_dht_list.resize(m_pps.NumCodingTable);

            {
                m_dht_list[0].TableClass = 0;
                m_dht_list[0].TableID    = 0;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[0].BITS[i] = DefaultLuminanceDCBits[i];
                for(mfxU16 i = 0; i < 12; i++)
                    m_dht_list[0].HUFFVAL[i] = DefaultLuminanceDCValues[i];
            }

            {
                m_dht_list[1].TableClass = 1;
                m_dht_list[1].TableID    = 0;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[1].BITS[i] = DefaultLuminanceACBits[i];
                for(mfxU16 i = 0; i < 162; i++)
                    m_dht_list[1].HUFFVAL[i] = DefaultLuminanceACValues[i];
            }
        }
        else
        {
            m_pps.NumCodingTable = 4;
            m_dht_list.resize(m_pps.NumCodingTable);

            {
                m_dht_list[0].TableClass = 0;
                m_dht_list[0].TableID    = 0;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[0].BITS[i] = DefaultLuminanceDCBits[i];
                for(mfxU16 i = 0; i < 12; i++)
                    m_dht_list[0].HUFFVAL[i] = DefaultLuminanceDCValues[i];
            }

            {
                m_dht_list[1].TableClass = 0;
                m_dht_list[1].TableID    = 1;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[1].BITS[i] = DefaultChrominanceDCBits[i];
                for(mfxU16 i = 0; i < 12; i++)
                    m_dht_list[1].HUFFVAL[i] = DefaultChrominanceDCValues[i];
            }

            {
                m_dht_list[2].TableClass = 1;
                m_dht_list[2].TableID    = 0;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[2].BITS[i] = DefaultLuminanceACBits[i];
                for(mfxU16 i = 0; i < 162; i++)
                    m_dht_list[2].HUFFVAL[i] = DefaultLuminanceACValues[i];
            }

            {
                m_dht_list[3].TableClass = 1;
                m_dht_list[3].TableID    = 1;
                for(mfxU16 i = 0; i < 16; i++)
                    m_dht_list[3].BITS[i] = DefaultChrominanceACBits[i];
                for(mfxU16 i = 0; i < 162; i++)
                    m_dht_list[3].HUFFVAL[i] = DefaultChrominanceACValues[i];
            }
        }
    }

    if (m_pps.NumCodingTable == 2)
    {
        m_scan_list[0].DcCodingTblSelector[0] = 0;
        m_scan_list[0].DcCodingTblSelector[1] = 0;
        m_scan_list[0].DcCodingTblSelector[2] = 0;
        m_scan_list[0].AcCodingTblSelector[0] = 0;
        m_scan_list[0].AcCodingTblSelector[1] = 0;
        m_scan_list[0].AcCodingTblSelector[2] = 0;
    }
    else if (m_pps.NumCodingTable == 4)
    {
        m_scan_list[0].DcCodingTblSelector[0] = 0;
        m_scan_list[0].DcCodingTblSelector[1] = 1;
        m_scan_list[0].DcCodingTblSelector[2] = 1;
        m_scan_list[0].AcCodingTblSelector[0] = 0;
        m_scan_list[0].AcCodingTblSelector[1] = 1;
        m_scan_list[0].AcCodingTblSelector[2] = 1;
    }

#elif defined (MFX_VA_LINUX)
    // Picture Header
    memset(&m_pps, 0, sizeof(m_pps));
    m_pps.reconstructed_picture = 0;
    m_pps.pic_flags.bits.profile = 0;
    m_pps.pic_flags.bits.progressive = 0;
    m_pps.pic_flags.bits.huffman = 1;
    m_pps.pic_flags.bits.interleaved   = (par->mfx.Interleaved != 0);
    m_pps.pic_flags.bits.differential  = 0;
    m_pps.picture_width       = (mfxU32)par->mfx.FrameInfo.CropW;
    m_pps.picture_height      = (mfxU32)par->mfx.FrameInfo.CropH;
    m_pps.sample_bit_depth = 8;
    m_pps.component_id[0] = 0;
    m_pps.component_id[1] = 1;
    m_pps.component_id[2] = 2;

    if (!jpegQT && !jpegQTInitial)
        m_pps.quality = (par->mfx.Quality > 100) ? 100 : par->mfx.Quality;

    if (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_YUV420)
        m_pps.num_components = 3;
    else if (fourCC == MFX_FOURCC_YUY2 && chromaFormat == MFX_CHROMAFORMAT_YUV422H)
        m_pps.num_components = 3;
    else if (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_YUV400)
        m_pps.num_components = 1;
    else if (fourCC == MFX_FOURCC_RGB4 && chromaFormat == MFX_CHROMAFORMAT_YUV444)
        m_pps.num_components = 3;
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // Scan Header
    m_pps.num_scan = 1;
    m_scan_list.resize(1);
    memset(&m_scan_list[0], 0, sizeof(m_scan_list[0]));
    m_scan_list[0].restart_interval = par->mfx.RestartInterval;
    m_scan_list[0].num_components = m_pps.num_components;
    m_scan_list[0].components[0].component_selector = 0;
    m_scan_list[0].components[1].component_selector = 1;
    m_scan_list[0].components[2].component_selector = 2;

    // Quanlization tables
    if (jpegQT || jpegQTInitial)
    {
        // External tables
        mfxExtJPEGQuantTables *pExtQuant = jpegQT ? jpegQT : jpegQTInitial;
        MFX_CHECK(pExtQuant->NumTable && (pExtQuant->NumTable <= hwCaps->MaxNumQuantTable), MFX_ERR_UNDEFINED_BEHAVIOR);
        m_dqt_list.resize(1);
        if(pExtQuant->NumTable == 1)
        {
            m_dqt_list[0].load_lum_quantiser_matrix = true;
            m_dqt_list[0].load_chroma_quantiser_matrix = false;
            for(mfxU16 i = 0; i< 64; i++)
            {
                m_dqt_list[0].lum_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[0][i];
            }
            m_pps.quantiser_table_selector[0] = 0;
            m_pps.quantiser_table_selector[1] = 0;
            m_pps.quantiser_table_selector[2] = 0;
        }
        else if(pExtQuant->NumTable == 2)
        {
            m_dqt_list[0].load_chroma_quantiser_matrix = true;
            m_dqt_list[0].load_lum_quantiser_matrix = true;
            for(mfxU16 i = 0; i< 64; i++)
            {
                m_dqt_list[0].lum_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[0][i];
                m_dqt_list[0].chroma_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[1][i];
            }
            m_pps.quantiser_table_selector[0] = 0;
            m_pps.quantiser_table_selector[1] = 1;
            m_pps.quantiser_table_selector[2] = 1;
        }
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    else
    {
        // No external tables - use Quality parameter
        m_dqt_list.resize(0);
        if (fourCC == MFX_FOURCC_RGB4)
        {
            m_pps.quantiser_table_selector[0] = 0;
            m_pps.quantiser_table_selector[1] = 0;
            m_pps.quantiser_table_selector[2] = 0;
        }
        else
        {
            m_pps.quantiser_table_selector[0] = 0;
            m_pps.quantiser_table_selector[1] = 1;
            m_pps.quantiser_table_selector[2] = 1;
        }
    }

    // Huffman tables
    if (jpegHT || jpegHTInitial)
    {
        // External tables
        mfxExtJPEGHuffmanTables *pExtHuffman = jpegHT ? jpegHT : jpegHTInitial;
        MFX_CHECK(pExtHuffman->NumDCTable &&
                  pExtHuffman->NumACTable &&
                 (pExtHuffman->NumDCTable < hwCaps->MaxNumHuffTable) &&
                 (pExtHuffman->NumACTable < hwCaps->MaxNumHuffTable), MFX_ERR_UNDEFINED_BEHAVIOR);
        m_dht_list.resize(1);
        for (mfxU16 j = 0; j < pExtHuffman->NumDCTable; j++)
        {
            if(j < 2)
            {
                MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].num_dc_codes, pExtHuffman->DCTables[j].Bits, 16 * sizeof(mfxU8));
                MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].dc_values, pExtHuffman->DCTables[j].Values, 12 * sizeof(mfxU8));
            }
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        for (mfxU16 j = 0; j < pExtHuffman->NumACTable; j++)
        {
            if(j < 2)
            {
                MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].num_ac_codes, pExtHuffman->ACTables[j].Bits, 16 * sizeof(mfxU8));
                MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].ac_values, pExtHuffman->ACTables[j].Values, 162 * sizeof(mfxU8));
            }
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    else
    {
        // Internal tables
        if (hwCaps->MaxNumHuffTable == 0)
        {
            m_dht_list.resize(0);
        }
        else if (hwCaps->MaxNumHuffTable == 1 || fourCC == MFX_FOURCC_RGB4)
        {
            m_dht_list.resize(1);

            m_dht_list[0].load_huffman_table[0] = 1;  //0 for luma

            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[0].num_dc_codes[i] = DefaultLuminanceDCBits[i];
            for(mfxU16 i = 0; i < 12; i++)
                m_dht_list[0].huffman_table[0].dc_values[i] = DefaultLuminanceDCValues[i];
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[0].num_ac_codes[i] = DefaultLuminanceACBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[0].huffman_table[0].ac_values[i] = DefaultLuminanceACValues[i];

            m_scan_list[0].components[0].dc_table_selector = 0;
            m_scan_list[0].components[1].dc_table_selector = 0;
            m_scan_list[0].components[2].dc_table_selector = 0;
            m_scan_list[0].components[0].ac_table_selector = 0;
            m_scan_list[0].components[1].ac_table_selector = 0;
            m_scan_list[0].components[2].ac_table_selector = 0;
        }
        else
        {
            m_dht_list.resize(1);

            m_dht_list[0].load_huffman_table[0] = 1;  //0 for luma
            m_dht_list[0].load_huffman_table[1] = 1;  // 1 for chroma

            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[0].num_dc_codes[i] = DefaultLuminanceDCBits[i];
            for(mfxU16 i = 0; i < 12; i++)
                m_dht_list[0].huffman_table[0].dc_values[i] = DefaultLuminanceDCValues[i];
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[0].num_ac_codes[i] = DefaultLuminanceACBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[0].huffman_table[0].ac_values[i] = DefaultLuminanceACValues[i];
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[1].num_dc_codes[i] = DefaultChrominanceDCBits[i];
            for(mfxU16 i = 0; i < 12; i++)
                m_dht_list[0].huffman_table[1].dc_values[i] = DefaultChrominanceDCValues[i];
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].huffman_table[1].num_ac_codes[i] = DefaultChrominanceACBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[0].huffman_table[1].ac_values[i] = DefaultChrominanceACValues[i];

            m_scan_list[0].components[0].dc_table_selector = 0;
            m_scan_list[0].components[1].dc_table_selector = 1;
            m_scan_list[0].components[2].dc_table_selector = 1;
            m_scan_list[0].components[0].ac_table_selector = 0;
            m_scan_list[0].components[1].ac_table_selector = 1;
            m_scan_list[0].components[2].ac_table_selector = 1;
        }
    }
#endif

    return sts;
}

void ExecuteBuffers::Close()
{
    if (m_payload_base.data)
    {
        delete [] m_payload_base.data;
        m_payload_base.data = 0;
        m_payload_base.length = 0;
        m_payload_base.maxLength = 0;
    }
    m_dht_list.clear();
    m_dqt_list.clear();
    m_scan_list.clear();
    m_payload_list.clear();
}

TaskManager::TaskManager()
{
    m_pTaskList = 0;
    m_TaskNum   = 0;
}

TaskManager::~TaskManager()
{
    Close();
}

mfxStatus TaskManager::Init(mfxU32 maxTaskNum)
{
    if (maxTaskNum > 0 &&
        maxTaskNum < JPEG_DDITASK_MAX_NUM)
    {
        m_TaskNum = maxTaskNum;
        m_pTaskList = new DdiTask[m_TaskNum];
        memset(m_pTaskList, 0, m_TaskNum * sizeof(DdiTask));
        for (mfxU32 i = 0; i < m_TaskNum; i++)
        {
            m_pTaskList[i].m_idx = i;
            m_pTaskList[i].m_idxBS = i;
        }
        return MFX_ERR_NONE;
    }
    else
    {
        m_pTaskList = 0;
        m_TaskNum   = 0;
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
}

mfxStatus TaskManager::Reset()
{
    if (m_pTaskList)
    {
        for (mfxU32 i = 0; i < m_TaskNum; i++)
        {
            if(m_pTaskList[i].m_pDdiData)
            {
                m_pTaskList[i].m_pDdiData->Close();
                delete m_pTaskList[i].m_pDdiData;
                m_pTaskList[i].m_pDdiData = NULL;
            }
            vm_interlocked_xchg32(&m_pTaskList[i].lInUse, 0);
            m_pTaskList[i].surface = 0;
            m_pTaskList[i].bs      = 0;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus TaskManager::Close()
{
    if (m_pTaskList)
    {
        for (mfxU32 i = 0; i < m_TaskNum; i++)
        {
            if(m_pTaskList[i].m_pDdiData)
            {
                m_pTaskList[i].m_pDdiData->Close();
                delete m_pTaskList[i].m_pDdiData;
                m_pTaskList[i].m_pDdiData = NULL;
            }
        }
        delete [] m_pTaskList;
        m_pTaskList = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus TaskManager::AssignTask(DdiTask *& newTask)
{
    if (m_pTaskList)
    {
        mfxU32 i;
        for (i = 0; i < m_TaskNum; i++)
        {
            if (m_pTaskList[i].lInUse == 0)
                break;
        }
        if (i < m_TaskNum)
        {
            newTask = &m_pTaskList[i];
            vm_interlocked_xchg32(&newTask->lInUse, 1);
            return MFX_ERR_NONE;
        }
        else
        {
            return MFX_WRN_DEVICE_BUSY;
        }
    }
    else
    {
        return MFX_ERR_NULL_PTR;
    }
}

mfxStatus TaskManager::RemoveTask(DdiTask & task)
{
    if (m_pTaskList)
    {
        vm_interlocked_xchg32(&task.lInUse, 0);
        task.surface = 0;
        task.bs      = 0;
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_NULL_PTR;
    }
}

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)
