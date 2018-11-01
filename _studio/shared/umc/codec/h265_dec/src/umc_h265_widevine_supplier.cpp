//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_h265_widevine_supplier.h"
#include "umc_h265_widevine_slice_decoding.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#ifdef UMC_VA_LINUX
#include "umc_va_linux_protected.h"
#endif

#include "mfx_common_int.h"

namespace UMC_HEVC_DECODER
{

const uint32_t levelIndexArray[] = {
    H265_LEVEL_1,
    H265_LEVEL_2,
    H265_LEVEL_21,

    H265_LEVEL_3,
    H265_LEVEL_31,

    H265_LEVEL_4,
    H265_LEVEL_41,

    H265_LEVEL_5,
    H265_LEVEL_51,
    H265_LEVEL_52,

    H265_LEVEL_6,
    H265_LEVEL_61,
    H265_LEVEL_62
};

WidevineTaskSupplier::WidevineTaskSupplier()
    : VATaskSupplier()
    , m_pWidevineDecrypter(new WidevineDecrypter{})
{}

WidevineTaskSupplier::~WidevineTaskSupplier()
{}

UMC::Status WidevineTaskSupplier::Init(UMC::VideoDecoderParams *pInit)
{
    UMC::Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Init();
        m_pWidevineDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);
    }

    return UMC::UMC_OK;
}

void WidevineTaskSupplier::Reset()
{
    VATaskSupplier::Reset();

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Reset();
    }
}

static
bool IsNeedSPSInvalidate(const H265SeqParamSet *old_sps, const H265SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->pic_width_in_luma_samples != new_sps->pic_width_in_luma_samples)
        return true;

    if (old_sps->pic_height_in_luma_samples != new_sps->pic_height_in_luma_samples)
        return true;

    if (old_sps->bit_depth_luma != new_sps->bit_depth_luma)
        return true;

    if (old_sps->bit_depth_chroma != new_sps->bit_depth_chroma)
        return true;

    if (old_sps->m_pcPTL.GetGeneralPTL()->profile_idc != new_sps->m_pcPTL.GetGeneralPTL()->profile_idc)
        return true;

    if (old_sps->chroma_format_idc != new_sps->chroma_format_idc)
        return true;

    if (old_sps->sps_max_dec_pic_buffering[0] < new_sps->sps_max_dec_pic_buffering[0])
        return true;

    return false;
}

UMC::Status WidevineTaskSupplier::ParseWidevineVPSSPSPPS(DecryptParametersWrapper* pDecryptParams)
{
    UMC::Status umcRes = UMC::UMC_OK;
    bool newResolution = false;

    try
    {
        //case NAL_UT_VPS:
        {
            H265VideoParamSet vps;
            vps.Reset();

            umcRes = pDecryptParams->GetVideoParamSet(&vps);
            if (umcRes != UMC::UMC_OK)
                return umcRes;

            m_Headers.m_VideoParams.AddHeader(&vps);
        }

        //case NAL_UT_SPS:
        {
            H265SeqParamSet sps;
            sps.Reset();

            umcRes = pDecryptParams->GetSequenceParamSet(&sps);
            if (umcRes != UMC::UMC_OK)
                return umcRes;

            if (sps.need16bitOutput && sps.m_pcPTL.GetGeneralPTL()->profile_idc == H265_PROFILE_MAIN10 && sps.bit_depth_luma == 8 && sps.bit_depth_chroma == 8 &&
                m_initializationParams.info.color_format == UMC::NV12)
                sps.need16bitOutput = 0;

            sps.WidthInCU = (sps.pic_width_in_luma_samples % sps.MaxCUSize) ? sps.pic_width_in_luma_samples / sps.MaxCUSize + 1 : sps.pic_width_in_luma_samples / sps.MaxCUSize;
            sps.HeightInCU = (sps.pic_height_in_luma_samples % sps.MaxCUSize) ? sps.pic_height_in_luma_samples / sps.MaxCUSize  + 1 : sps.pic_height_in_luma_samples / sps.MaxCUSize;
            sps.NumPartitionsInCUSize = 1 << sps.MaxCUDepth;
            sps.NumPartitionsInCU = 1 << (sps.MaxCUDepth << 1);
            sps.NumPartitionsInFrameWidth = sps.WidthInCU * sps.NumPartitionsInCUSize;

            uint8_t newDPBsize = (uint8_t)CalculateDPBSize(sps.getPTL()->GetGeneralPTL()->profile_idc, sps.getPTL()->GetGeneralPTL()->level_idc,
                                        sps.pic_width_in_luma_samples,
                                        sps.pic_height_in_luma_samples,
                                        sps.sps_max_dec_pic_buffering[HighestTid]);

            for (uint32_t i = 0; i <= HighestTid; i++)
            {
                if (sps.sps_max_dec_pic_buffering[i] > newDPBsize)
                    sps.sps_max_dec_pic_buffering[i] = newDPBsize;
            }

            HighestTid = sps.sps_max_sub_layers - 1;

            if (!sps.getPTL()->GetGeneralPTL()->level_idc && sps.sps_max_dec_pic_buffering[HighestTid])
            {
                uint32_t level_idc = levelIndexArray[0];
                for (size_t i = 0; i < sizeof(levelIndexArray)/sizeof(levelIndexArray[0]); i++)
                {
                    level_idc = levelIndexArray[i];
                    newDPBsize = (uint8_t)CalculateDPBSize(sps.getPTL()->GetGeneralPTL()->profile_idc, level_idc,
                                            sps.pic_width_in_luma_samples,
                                            sps.pic_height_in_luma_samples,
                                            sps.sps_max_dec_pic_buffering[HighestTid]);

                    if (newDPBsize >= sps.sps_max_dec_pic_buffering[HighestTid])
                        break;
                }

                sps.getPTL()->GetGeneralPTL()->level_idc = level_idc;
            }

            sps.sps_max_dec_pic_buffering[0] = sps.sps_max_dec_pic_buffering[HighestTid] ? sps.sps_max_dec_pic_buffering[HighestTid] : newDPBsize;

            const H265SeqParamSet * old_sps = m_Headers.m_SeqParams.GetCurrentHeader();
            if (IsNeedSPSInvalidate(old_sps, &sps))
            {
                newResolution = true;
            }

            m_Headers.m_SeqParams.AddHeader(&sps);
        }

        //case NAL_UT_PPS:
        {
            H265PicParamSet pps;
            pps.Reset();

            umcRes = pDecryptParams->GetPictureParamSetFull(&pps);
            if (umcRes != UMC::UMC_OK)
                return umcRes;

            // Initialize tiles data
            H265SeqParamSet *sps = m_Headers.m_SeqParams.GetHeader(pps.pps_seq_parameter_set_id);
            if (!sps)
                return UMC::UMC_ERR_INVALID_STREAM;

            // additional PPS members checks:
            int32_t QpBdOffsetY = 6 * (sps->bit_depth_luma - 8);
            if (pps.init_qp < -QpBdOffsetY || pps.init_qp > 25 + 26)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.cross_component_prediction_enabled_flag && sps->ChromaArrayType != CHROMA_FORMAT_444)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.chroma_qp_offset_list_enabled_flag && sps->ChromaArrayType == CHROMA_FORMAT_400)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.diff_cu_qp_delta_depth > sps->log2_max_luma_coding_block_size - sps->log2_min_luma_coding_block_size)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.log2_parallel_merge_level > sps->log2_max_luma_coding_block_size)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.log2_sao_offset_scale_luma > sps->bit_depth_luma - 10)
                return UMC::UMC_ERR_INVALID_STREAM;

            if (pps.log2_sao_offset_scale_chroma  > sps->bit_depth_chroma - 10)
                return UMC::UMC_ERR_INVALID_STREAM;

            uint32_t WidthInLCU = sps->WidthInCU;
            uint32_t HeightInLCU = sps->HeightInCU;

            pps.m_CtbAddrRStoTS.resize(WidthInLCU * HeightInLCU + 1);
            pps.m_CtbAddrTStoRS.resize(WidthInLCU * HeightInLCU + 1);
            pps.m_TileIdx.resize(WidthInLCU * HeightInLCU + 1);

            // Calculate tiles rows and columns coordinates
            if (pps.tiles_enabled_flag)
            {
                uint32_t columns = pps.num_tile_columns;
                uint32_t rows = pps.num_tile_rows;

                if (columns > WidthInLCU)
                    return UMC::UMC_ERR_INVALID_STREAM;

                if (rows > HeightInLCU)
                    return UMC::UMC_ERR_INVALID_STREAM;

                if (pps.uniform_spacing_flag)
                {
                    uint32_t lastDiv, i;

                    pps.column_width.resize(columns);
                    pps.row_height.resize(rows);

                    lastDiv = 0;
                    for (i = 0; i < columns; i++)
                    {
                        uint32_t tmp = ((i + 1) * WidthInLCU) / columns;
                        pps.column_width[i] = tmp - lastDiv;
                        lastDiv = tmp;
                    }

                    lastDiv = 0;
                    for (i = 0; i < rows; i++)
                    {
                        uint32_t tmp = ((i + 1) * HeightInLCU) / rows;
                        pps.row_height[i] = tmp - lastDiv;
                        lastDiv = tmp;
                    }
                }
                else
                {
                    // Initialize last column/row values, all previous values were parsed from PPS header
                    uint32_t i;
                    uint32_t tmp = 0;

                    for (i = 0; i < pps.num_tile_columns - 1; i++)
                    {
                        uint32_t column = pps.getColumnWidth(i);
                        if (column > WidthInLCU)
                            return UMC::UMC_ERR_INVALID_STREAM;
                        tmp += column;
                    }

                    if (tmp >= WidthInLCU)
                        return UMC::UMC_ERR_INVALID_STREAM;

                    pps.column_width[pps.num_tile_columns - 1] = WidthInLCU - tmp;

                    tmp = 0;
                    for (i = 0; i < pps.num_tile_rows - 1; i++)
                    {
                        uint32_t row = pps.getRowHeight(i);
                        if (row > HeightInLCU)
                            return UMC::UMC_ERR_INVALID_STREAM;
                        tmp += row;
                    }

                    pps.row_height[pps.num_tile_rows - 1] = HeightInLCU - tmp;
                    if (tmp >= HeightInLCU)
                        return UMC::UMC_ERR_INVALID_STREAM;
                }

                uint32_t *colBd = h265_new_array_throw<uint32_t>(columns);
                uint32_t *rowBd = h265_new_array_throw<uint32_t>(rows);

                colBd[0] = 0;
                for (uint32_t i = 0; i < pps.num_tile_columns - 1; i++)
                {
                    colBd[i + 1] = colBd[i] + pps.getColumnWidth(i);
                }

                rowBd[0] = 0;
                for (uint32_t i = 0; i < pps.num_tile_rows - 1; i++)
                {
                    rowBd[i + 1] = rowBd[i] + pps.getRowHeight(i);
                }

                uint32_t tbX = 0, tbY = 0;

                // Initialize CTB index raster to tile and inverse lookup tables
                for (uint32_t i = 0; i < WidthInLCU * HeightInLCU; i++)
                {
                    uint32_t tileX = 0, tileY = 0, CtbAddrRStoTS;

                    for (uint32_t j = 0; j < columns; j++)
                    {
                        if (tbX >= colBd[j])
                        {
                            tileX = j;
                        }
                    }

                    for (uint32_t j = 0; j < rows; j++)
                    {
                        if (tbY >= rowBd[j])
                        {
                            tileY = j;
                        }
                    }

                    CtbAddrRStoTS = WidthInLCU * rowBd[tileY] + pps.getRowHeight(tileY) * colBd[tileX];
                    CtbAddrRStoTS += (tbY - rowBd[tileY]) * pps.getColumnWidth(tileX) + tbX - colBd[tileX];

                    pps.m_CtbAddrRStoTS[i] = CtbAddrRStoTS;
                    pps.m_TileIdx[i] = tileY * columns + tileX;

                    tbX++;
                    if (tbX == WidthInLCU)
                    {
                        tbX = 0;
                        tbY++;
                    }
                }

                for (uint32_t i = 0; i < WidthInLCU * HeightInLCU; i++)
                {
                    uint32_t CtbAddrRStoTS = pps.m_CtbAddrRStoTS[i];
                    pps.m_CtbAddrTStoRS[CtbAddrRStoTS] = i;
                }

                pps.m_CtbAddrRStoTS[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;
                pps.m_CtbAddrTStoRS[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;
                pps.m_TileIdx[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;

                delete[] colBd;
                delete[] rowBd;
            }
            else
            {
                for (uint32_t i = 0; i < WidthInLCU * HeightInLCU + 1; i++)
                {
                    pps.m_CtbAddrTStoRS[i] = i;
                    pps.m_CtbAddrRStoTS[i] = i;
                }
                fill(pps.m_TileIdx.begin(), pps.m_TileIdx.end(), 0);
            }

            int32_t numberOfTiles = pps.tiles_enabled_flag ? pps.getNumTiles() : 0;

            pps.tilesInfo.resize(numberOfTiles);

            if (!numberOfTiles)
            {
                pps.tilesInfo.resize(1);
                pps.tilesInfo[0].firstCUAddr = 0;
                pps.tilesInfo[0].endCUAddr = WidthInLCU*HeightInLCU;
            }

            // Initialize tiles coordinates
            for (int32_t i = 0; i < numberOfTiles; i++)
            {
                int32_t tileY = i / pps.num_tile_columns;
                int32_t tileX = i % pps.num_tile_columns;

                int32_t startY = 0;
                int32_t startX = 0;

                for (int32_t j = 0; j < tileX; j++)
                {
                    startX += pps.column_width[j];
                }

                for (int32_t j = 0; j < tileY; j++)
                {
                    startY += pps.row_height[j];
                }

                pps.tilesInfo[i].endCUAddr = (startY + pps.row_height[tileY] - 1)* WidthInLCU + startX + pps.column_width[tileX] - 1;
                pps.tilesInfo[i].firstCUAddr = startY * WidthInLCU + startX;
                pps.tilesInfo[i].width = pps.column_width[tileX];
            }

            m_Headers.m_PicParams.AddHeader(&pps);
        }
    }
    catch(const h265_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC::UMC_ERR_INVALID_STREAM;
    }

    return newResolution ? UMC::UMC_NTF_NEW_RESOLUTION : UMC::UMC_OK;
}

H265Slice *WidevineTaskSupplier::ParseWidevineSliceHeader(DecryptParametersWrapper* pDecryptParams)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    H265WidevineSlice * pSlice = m_ObjHeap.AllocateObject<H265WidevineSlice>();
    pSlice->IncrementReference();

    notifier0<H265WidevineSlice> memory_leak_preventing_slice(pSlice, &H265WidevineSlice::DecrementReference);

    pSlice->SetDecryptParameters(pDecryptParams);
    pSlice->m_source.SetTime(pDecryptParams->GetTime());

    int32_t pps_pid = pSlice->RetrievePicParamSetNumber();
    if (pps_pid == -1)
    {
        return 0;
    }

    pSlice->SetPicParam(m_Headers.m_PicParams.GetHeader(pps_pid));
    H265PicParamSet const* pps = pSlice->GetPicParam();
    if (!pps)
    {
        return 0;
    }

    int32_t seq_parameter_set_id = pps->pps_seq_parameter_set_id;

    pSlice->SetSeqParam(m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id));
    if (!pSlice->GetSeqParam())
    {
        return 0;
    }

    m_Headers.m_SeqParams.SetCurrentID(pps->pps_seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pps->pps_pic_parameter_set_id);

    pSlice->m_pCurrentFrame = NULL;

    bool ready = pSlice->Reset(&m_pocDecoding);
    if (!ready)
    {
        m_prevSliceBroken = pSlice->IsError();
        return 0;
    }

    H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();
    VM_ASSERT(sliceHdr);

    if (m_prevSliceBroken && sliceHdr->dependent_slice_segment_flag)
    {
        //Prev. slice contains errors. There is no relayable way to infer parameters for dependent slice
        return 0;
    }

    m_prevSliceBroken = false;

    if (m_WaitForIDR)
    {
        if (pps->pps_curr_pic_ref_enabled_flag)
        {
            ReferencePictureSet const* rps = pSlice->getRPS();
            VM_ASSERT(rps);

            uint32_t const numPicTotalCurr = rps->getNumberOfUsedPictures();
            if (numPicTotalCurr)
                return 0;
        }
        else if (sliceHdr->slice_type != I_SLICE)
        {
            return 0;
        }
    }

    ActivateHeaders(const_cast<H265SeqParamSet *>(pSlice->GetSeqParam()), const_cast<H265PicParamSet *>(pps));

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    pSlice->SetWidevineStatusReportNumber(pDecryptParams->ui16StatusReportFeebackNumber);

    //for SliceIdx m_SliceIdx m_iNumber
    pSlice->m_iNumber = m_SliceIdxInTaskSupplier;
    m_SliceIdxInTaskSupplier++;
    return pSlice;
}

UMC::Status WidevineTaskSupplier::ParseWidevineSEI(DecryptParametersWrapper* pDecryptParams)
{
    if (m_Headers.m_SeqParams.GetCurrentID() == -1)
        return UMC::UMC_OK;

    try
    {
        {
            H265SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIBufferingPeriod(m_Headers, &m_SEIPayLoads);
            m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
        }
        {
            H265SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIPicTiming(m_Headers, &m_SEIPayLoads);
            m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
        }
    }
    catch(...)
    {
        // nothing to do just catch it
    }

    return UMC::UMC_OK;
}

UMC::Status WidevineTaskSupplier::AddOneFrame(UMC::MediaData* src)
{
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return UMC::UMC_ERR_FAILED;
    }

    UMC::Status umsRes = UMC::UMC_OK;

    if (m_pLastSlice)
    {
        umsRes = AddSlice(m_pLastSlice, !src);
        if (umsRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC::UMC_OK)
            return umsRes;
    }

    uint32_t const flags = src ? src->GetFlags() : 0;
    if (flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
        return UMC::UMC_ERR_FAILED;

    bool decrypted = false;
    DecryptParametersWrapper decryptParams{};

    auto aux = src ? src->GetAuxInfo(MFX_EXTBUFF_DECRYPTED_PARAM) : nullptr;
    auto dp_ext = aux ? reinterpret_cast<mfxExtDecryptedParam*>(aux->ptr) : nullptr;

    if (dp_ext)
    {
        if (!dp_ext->Data       ||
             dp_ext->DataLength != sizeof (DECRYPT_QUERY_STATUS_PARAMS_HEVC))
            return UMC::UMC_ERR_FAILED;

        decryptParams = *(reinterpret_cast<DECRYPT_QUERY_STATUS_PARAMS_HEVC*>(dp_ext->Data));
        decrypted = true;
    }
    else
    {
        void* bsDataPointer = src ? src->GetDataPointer() : 0;

        umsRes = m_pWidevineDecrypter->DecryptFrame(src, &decryptParams);
        if (umsRes != UMC::UMC_OK)
            return umsRes;

        decrypted = src ? (bsDataPointer != src->GetDataPointer()) : false;
    }

    decryptParams.SetTime(src ? src->GetTime() : 0);

    if (decrypted)
    {
        umsRes = ParseWidevineVPSSPSPPS(&decryptParams);
        if (umsRes != UMC::UMC_OK)
            return umsRes;

        umsRes = ParseWidevineSEI(&decryptParams);
        if (umsRes != UMC::UMC_OK)
            return umsRes;

        H265Slice * pSlice = ParseWidevineSliceHeader(&decryptParams);
        if (pSlice)
        {
            umsRes = AddSlice(pSlice, !src);
            if (umsRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC::UMC_OK)
                return umsRes;
        }
    }

    return AddSlice(0, !src);
}

void WidevineTaskSupplier::CompleteFrame(H265DecoderFrame * pFrame)
{
    VATaskSupplier::CompleteFrame(pFrame);

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->ReleaseForNewBitstream();
    }
}

} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H265_VIDEO_DECODER
