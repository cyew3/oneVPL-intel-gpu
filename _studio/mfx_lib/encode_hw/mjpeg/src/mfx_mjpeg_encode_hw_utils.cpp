/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
*/

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

    if (core && core->GetVAType() == MFX_HW_VAAPI && core->GetHWType() < MFX_HW_CHV)
        return MFX_ERR_UNSUPPORTED;

    std::auto_ptr<DriverEncoder> ddi;
    ddi.reset( CreatePlatformMJpegEncoder(core) );

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

mfxStatus MfxHwMJpegEncode::CheckJpegParam(mfxVideoParam & par,
                                           JpegEncCaps const & hwCaps)
{
    MFX_CHECK(hwCaps.Baseline, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.Sequential, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(hwCaps.Huffman, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    MFX_CHECK(par.mfx.Interleaved && hwCaps.Interleaved || !par.mfx.Interleaved && hwCaps.NonInterleaved,
        MFX_WRN_PARTIAL_ACCELERATION);

    MFX_CHECK(par.mfx.FrameInfo.Width > 0 && par.mfx.FrameInfo.Height > 0,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

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
    bool      bExternalFrameLocked = false;

    core->LockFrame(vidMemId, &vidSurf);
    MFX_CHECK(vidSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
    if(sysSurf.Y == 0){ // need to lock the surface
        core->LockExternalFrame(sysSurf.MemId, &sysSurf);
        bExternalFrameLocked = true;
    }
    MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy input (sys->vid)");
        mfxFrameSurface1 surfSrc = { {0,}, frmInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {0,}, frmInfo, vidSurf };
        sts = core->DoFastCopy(&surfDst, &surfSrc);
        MFX_CHECK_STS(sts);
    }
    if(bExternalFrameLocked) {
        sts = core->UnlockExternalFrame(sysSurf.MemId, &sysSurf);
        MFX_CHECK_STS(sts);
    }
    sts = core->UnlockFrame(vidMemId, &vidSurf);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus ExecuteBuffers::Init(mfxVideoParam const *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    memset(&m_payload, 0, sizeof(m_payload));
    m_payload_data_present = false;

#if defined (MFX_VA_WIN)
    memset(&m_pps, 0, sizeof(m_pps));

    // Picture Header
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
    m_pps.ComponentID[3] = 3;
    m_pps.QuantTableSelector[0] = 0;
    m_pps.QuantTableSelector[1] = 1;
    m_pps.QuantTableSelector[2] = 1;
    m_pps.QuantTableSelector[3] = 1;
    m_pps.Quality = (par->mfx.Quality > 100) ? 100 : par->mfx.Quality;

    mfxU32 fourCC = par->mfx.FrameInfo.FourCC;
    mfxU16 chromaFormat = par->mfx.FrameInfo.ChromaFormat;
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
    m_scan_list[0].RestartInterval = par->mfx.RestartInterval;
    m_scan_list[0].NumComponent = m_pps.NumComponent;
    m_scan_list[0].ComponentSelector[0] = 0;
    m_scan_list[0].ComponentSelector[1] = 1;
    m_scan_list[0].ComponentSelector[2] = 2;
    m_scan_list[0].DcCodingTblSelector[0] = 0;
    m_scan_list[0].DcCodingTblSelector[1] = 1;
    m_scan_list[0].DcCodingTblSelector[2] = 1;
    m_scan_list[0].AcCodingTblSelector[0] = 0;
    m_scan_list[0].AcCodingTblSelector[1] = 1;
    m_scan_list[0].AcCodingTblSelector[2] = 1;
    m_scan_list[0].FirstDCTCoeff = 0;
    m_scan_list[0].LastDCTCoeff = 0;
    m_scan_list[0].Ah = 0;
    m_scan_list[0].Al = 0;

    // Quantization Table
    //{
    //    m_pps.NumQuantTable = 2;
    //    m_dqt_list.resize(m_pps.NumQuantTable);

    //    {
    //        m_dqt_list[0].TableID   = 0;
    //        m_dqt_list[0].Precision = 0;
    //        for(mfxU16 i = 0; i < 64; i++)
    //            m_dqt_list[0].Qm[i] = DefaultLuminanceQuant[i];
    //    }

    //    {
    //        m_dqt_list[1].TableID   = 1;
    //        m_dqt_list[1].Precision = 0;
    //        for(mfxU16 i = 0; i < 64; i++)
    //            m_dqt_list[1].Qm[i] = DefaultChrominanceQuant[i];
    //    }
    //}

    // Huffman Table
    m_pps.NumCodingTable = 4;
    m_dht_list.resize(m_pps.NumCodingTable);

    {
        m_dht_list[0].TableClass = 0;
        m_dht_list[0].TableID    = 0;
        for(mfxU16 i = 0; i < 16; i++)
            m_dht_list[0].BITS[i] = DefaultLuminanceDCBits[i];
        for(mfxU16 i = 0; i < 162; i++)
            m_dht_list[0].HUFFVAL[i] = DefaultLuminanceDCValues[i];
    }

    {
        m_dht_list[1].TableClass = 0;
        m_dht_list[1].TableID    = 1;
        for(mfxU16 i = 0; i < 16; i++)
            m_dht_list[1].BITS[i] = DefaultChrominanceDCBits[i];
        for(mfxU16 i = 0; i < 162; i++)
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

    // fetch extention buffers if existed
    if (par->ExtParam != 0)
    {
        for (mfxU16 i = 0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i] != 0)
            {
                switch (par->ExtParam[i]->BufferId)
                {
                case MFX_EXTBUFF_JPEG_QT:
                    {
                        mfxExtJPEGQuantTables *pExtQuant = (mfxExtJPEGQuantTables*)par->ExtParam[i];
                        m_pps.NumQuantTable = (UINT)pExtQuant->NumTable;
                        if (m_pps.NumQuantTable)
                        {
                            m_dqt_list.resize(m_pps.NumQuantTable);
                            for (mfxU16 j = 0; j < m_pps.NumQuantTable; j++)
                            {
                                m_dqt_list[j].TableID   = j;
                                m_dqt_list[j].Precision = 0; /* (UINT)pExtQuant->Precision[j];*/
                                MFX_INTERNAL_CPY(m_dqt_list[j].Qm, &pExtQuant->Qm[j], 64 * sizeof(USHORT));
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;

                case MFX_EXTBUFF_JPEG_HUFFMAN:
                    {
                        mfxExtJPEGHuffmanTables *pExtHuffman = (mfxExtJPEGHuffmanTables*)par->ExtParam[i];
                        m_pps.NumCodingTable = (UINT)(pExtHuffman->NumDCTable + pExtHuffman->NumACTable);
                        if (m_pps.NumCodingTable)
                        {
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
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;

#if 0                    
                case MFX_EXTBUFF_JPEG_PAYLOAD:
                    {
                        mfxExtJPEGPayloadData *pExtPayload = (mfxExtJPEGPayloadData*)par->ExtParam[i];
                        if (pExtPayload->Data && pExtPayload->NumByte > 0)
                        {
                            if (m_payload.data && m_payload.size < pExtPayload->NumByte)
                            {
                                delete [] m_payload.data;
                                m_payload.data = 0;
                            }
                            if (!m_payload.data)
                            {
                                m_payload.data = new UCHAR[pExtPayload->NumByte];
                            }
                            if (m_payload.data)
                            {
                                MFX_INTERNAL_CPY(m_payload.data, pExtPayload->Data, pExtPayload->NumByte);
                                m_payload.size = pExtPayload->NumByte;
                                m_payload_data_present = true;
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;
#endif

                default:
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                    break;
                }
            }
        }
    }

#elif defined (MFX_VA_LINUX)
    memset(&m_pps, 0, sizeof(m_pps));

    // Picture Header
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
    m_pps.component_id[3] = 3;
    m_pps.quantiser_table_selector[0] = 0;
    m_pps.quantiser_table_selector[1] = 1;
    m_pps.quantiser_table_selector[2] = 1;
    m_pps.quantiser_table_selector[3] = 1;
    m_pps.quality = (par->mfx.Quality > 100) ? 100 : par->mfx.Quality;

    mfxU32 fourCC = par->mfx.FrameInfo.FourCC;
    mfxU16 chromaFormat = par->mfx.FrameInfo.ChromaFormat;
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
    m_scan_list[0].restart_interval = par->mfx.RestartInterval;
    m_scan_list[0].num_components = m_pps.num_components;
    m_scan_list[0].components[0].component_selector = 0;
    m_scan_list[0].components[0].dc_table_selector = 0;
    m_scan_list[0].components[0].ac_table_selector = 0;
    m_scan_list[0].components[1].component_selector = 1;
    m_scan_list[0].components[1].dc_table_selector = 1;
    m_scan_list[0].components[1].ac_table_selector = 1;
    m_scan_list[0].components[2].component_selector = 2;
    m_scan_list[0].components[2].dc_table_selector = 1;
    m_scan_list[0].components[2].ac_table_selector = 1;

    // Huffman Table
    m_dht_list.resize(1);

    m_dht_list[0].load_huffman_table[0] = 1;  //0 for luma
    m_dht_list[0].load_huffman_table[1] = 1;  // 1 for chroma
    for(mfxU16 i = 0; i < 16; i++){
        m_dht_list[0].huffman_table[0].num_dc_codes[i] = DefaultLuminanceDCBits[i];
    }
    for(mfxU16 i = 0; i < 162; i++) {
        m_dht_list[0].huffman_table[0].dc_values[i] = DefaultLuminanceDCValues[i];
    }
    for(mfxU16 i = 0; i < 16; i++){
        m_dht_list[0].huffman_table[0].num_ac_codes[i] = DefaultLuminanceACBits[i];
    }
    for(mfxU16 i = 0; i < 162; i++) {
        m_dht_list[0].huffman_table[0].ac_values[i] = DefaultLuminanceACValues[i];
    }
    for(mfxU16 i = 0; i < 16; i++){
        m_dht_list[0].huffman_table[1].num_dc_codes[i] = DefaultChrominanceDCBits[i];
    }
    for(mfxU16 i = 0; i < 162; i++) {
        m_dht_list[0].huffman_table[1].dc_values[i] = DefaultChrominanceDCValues[i];
    }
    for(mfxU16 i = 0; i < 16; i++){
        m_dht_list[0].huffman_table[1].num_ac_codes[i] = DefaultChrominanceACBits[i];
    }
    for(mfxU16 i = 0; i < 162; i++) {
        m_dht_list[0].huffman_table[1].ac_values[i] = DefaultChrominanceACValues[i];
    }

    //m_dqt_list.resize(1);

    //m_dqt_list[0].load_lum_quantiser_matrix = 1;
    //for(mfxU16 i = 0; i< 64; i++){
    //    m_dqt_list[0].lum_quantiser_matrix[i]= DefaultLuminanceQuant[i];
    //}
    //m_dqt_list[0].load_chroma_quantiser_matrix = 1;
    //for(mfxU16 i = 0; i< 64; i++){
    //    m_dqt_list[0].chroma_quantiser_matrix[i]=(unsigned char) DefaultChrominanceQuant[i];
    //}

    if (par->ExtParam != 0)
    {
        for (mfxU16 i = 0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i] != 0)
            {
                switch (par->ExtParam[i]->BufferId)
                {
                case MFX_EXTBUFF_JPEG_QT:
                    {
                        mfxExtJPEGQuantTables *pExtQuant = (mfxExtJPEGQuantTables*)par->ExtParam[i];
                        if(pExtQuant->NumTable == 1) {
                            m_dqt_list[0].load_lum_quantiser_matrix = true;
                            m_dqt_list[0].load_chroma_quantiser_matrix = false;
                            for(mfxU16 i = 0; i< 64; i++){
                                m_dqt_list[0].lum_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[0][i];
                            }
                        }
                        else if(pExtQuant->NumTable == 2) {
                            m_dqt_list[0].load_chroma_quantiser_matrix = true;
                            m_dqt_list[0].load_lum_quantiser_matrix = true;
                            for(mfxU16 i = 0; i< 64; i++){
                                m_dqt_list[0].lum_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[0][i];
                                m_dqt_list[0].chroma_quantiser_matrix[i]=(unsigned char) pExtQuant->Qm[1][i];
                            }
                        }
                        else {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }

                    }
                    break;

                case MFX_EXTBUFF_JPEG_HUFFMAN:
                    {
                        mfxExtJPEGHuffmanTables *pExtHuffman = (mfxExtJPEGHuffmanTables*)par->ExtParam[i];
                        if (pExtHuffman->NumACTable || pExtHuffman->NumDCTable)
                        {
                            for (mfxU16 j = 0; j < pExtHuffman->NumDCTable; j++)
                            {
                                if(j < 2) {
                                    MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].num_dc_codes, pExtHuffman->DCTables[j].Bits, 16 * sizeof(mfxU8));
                                    MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].dc_values, pExtHuffman->DCTables[j].Values, 12 * sizeof(mfxU8));
                                }
                                else {
                                    return MFX_ERR_INVALID_VIDEO_PARAM;
                                }
                            }
                            for (mfxU16 j = 0; j < pExtHuffman->NumACTable; j++)
                            {
                                if(j < 2) {
                                    MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].num_ac_codes, pExtHuffman->ACTables[j].Bits, 16 * sizeof(mfxU8));
                                    MFX_INTERNAL_CPY(m_dht_list[0].huffman_table[j].ac_values, pExtHuffman->ACTables[j].Values, 12 * sizeof(mfxU8));
                                }
                                else {
                                    return MFX_ERR_INVALID_VIDEO_PARAM;
                                }
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;


                default:
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                    break;
                }
            }
        }
    }

#endif

    return sts;
}

void ExecuteBuffers::Close()
{
    if (m_payload.data)
    {
        delete [] m_payload.data;
        m_payload.data = 0;
    }
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
