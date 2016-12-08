//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_h265_widevine_supplier.h"

#ifndef MFX_PROTECTED_FEATURE_DISABLE

#include "umc_h265_widevine_slice_decoding.h"

#include "umc_va_dxva2_protected.h"
#include "umc_va_linux_protected.h"

namespace UMC_HEVC_DECODER
{

const Ipp32u levelIndexArray[] = {
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

WidevineTaskSupplier::WidevineTaskSupplier():
    VATaskSupplier(),
    m_pWidevineDecrypter(0)
{
}

WidevineTaskSupplier::~WidevineTaskSupplier()
{
    if(m_pWidevineDecrypter)
        delete m_pWidevineDecrypter;
}

UMC::Status WidevineTaskSupplier::Init(UMC::VideoDecoderParams *pInit)
{
    UMC::Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter = new WidevineDecrypter;
        m_pWidevineDecrypter->Init();
        m_pWidevineDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);
    }
#endif

    return UMC::UMC_OK;
}

void WidevineTaskSupplier::Reset()
{
    VATaskSupplier::Reset();

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Reset();
    }
#endif
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

            Ipp8u newDPBsize = (Ipp8u)CalculateDPBSize(sps.getPTL()->GetGeneralPTL()->level_idc,
                                        sps.pic_width_in_luma_samples,
                                        sps.pic_height_in_luma_samples,
                                        sps.sps_max_dec_pic_buffering[HighestTid]);

            for (Ipp32u i = 0; i <= HighestTid; i++)
            {
                if (sps.sps_max_dec_pic_buffering[i] > newDPBsize)
                    sps.sps_max_dec_pic_buffering[i] = newDPBsize;
            }

            HighestTid = sps.sps_max_sub_layers - 1;

            if (!sps.getPTL()->GetGeneralPTL()->level_idc && sps.sps_max_dec_pic_buffering[HighestTid])
            {
                Ipp32u level_idc = levelIndexArray[0];
                for (size_t i = 0; i < sizeof(levelIndexArray)/sizeof(levelIndexArray[0]); i++)
                {
                    level_idc = levelIndexArray[i];
                    newDPBsize = (Ipp8u)CalculateDPBSize(level_idc,
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
            Ipp32s QpBdOffsetY = 6 * (sps->bit_depth_luma - 8);
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

            Ipp32u WidthInLCU = sps->WidthInCU;
            Ipp32u HeightInLCU = sps->HeightInCU;

            pps.m_CtbAddrRStoTS = h265_new_array_throw<Ipp32u>(WidthInLCU * HeightInLCU + 1);
            if (NULL == pps.m_CtbAddrRStoTS)
            {
                pps.Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            pps.m_CtbAddrTStoRS = h265_new_array_throw<Ipp32u>(WidthInLCU * HeightInLCU + 1);
            if (NULL == pps.m_CtbAddrTStoRS)
            {
                pps.Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            pps.m_TileIdx = h265_new_array_throw<Ipp32u>(WidthInLCU * HeightInLCU + 1);
            if (NULL == pps.m_TileIdx)
            {
                pps.Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            // Calculate tiles rows and columns coordinates
            if (pps.tiles_enabled_flag)
            {
                Ipp32u columns = pps.num_tile_columns;
                Ipp32u rows = pps.num_tile_rows;

                if (columns > WidthInLCU)
                    return UMC::UMC_ERR_INVALID_STREAM;

                if (rows > HeightInLCU)
                    return UMC::UMC_ERR_INVALID_STREAM;

                if (pps.uniform_spacing_flag)
                {
                    Ipp32u lastDiv, i;

                    pps.column_width = h265_new_array_throw<Ipp32u>(columns);
                    pps.row_height = h265_new_array_throw<Ipp32u>(rows);

                    lastDiv = 0;
                    for (i = 0; i < columns; i++)
                    {
                        Ipp32u tmp = ((i + 1) * WidthInLCU) / columns;
                        pps.column_width[i] = tmp - lastDiv;
                        lastDiv = tmp;
                    }

                    lastDiv = 0;
                    for (i = 0; i < rows; i++)
                    {
                        Ipp32u tmp = ((i + 1) * HeightInLCU) / rows;
                        pps.row_height[i] = tmp - lastDiv;
                        lastDiv = tmp;
                    }
                }
                else
                {
                    // Initialize last column/row values, all previous values were parsed from PPS header
                    Ipp32u i;
                    Ipp32u tmp = 0;

                    for (i = 0; i < pps.num_tile_columns - 1; i++)
                    {
                        Ipp32u column = pps.getColumnWidth(i);
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
                        Ipp32u row = pps.getRowHeight(i);
                        if (row > HeightInLCU)
                            return UMC::UMC_ERR_INVALID_STREAM;
                        tmp += row;
                    }

                    pps.row_height[pps.num_tile_rows - 1] = HeightInLCU - tmp;
                    if (tmp >= HeightInLCU)
                        return UMC::UMC_ERR_INVALID_STREAM;
                }

                Ipp32u *colBd = h265_new_array_throw<Ipp32u>(columns);
                Ipp32u *rowBd = h265_new_array_throw<Ipp32u>(rows);

                colBd[0] = 0;
                for (Ipp32u i = 0; i < pps.num_tile_columns - 1; i++)
                {
                    colBd[i + 1] = colBd[i] + pps.getColumnWidth(i);
                }

                rowBd[0] = 0;
                for (Ipp32u i = 0; i < pps.num_tile_rows - 1; i++)
                {
                    rowBd[i + 1] = rowBd[i] + pps.getRowHeight(i);
                }

                Ipp32u tbX = 0, tbY = 0;

                // Initialize CTB index raster to tile and inverse lookup tables
                for (Ipp32u i = 0; i < WidthInLCU * HeightInLCU; i++)
                {
                    Ipp32u tileX = 0, tileY = 0, CtbAddrRStoTS;

                    for (Ipp32u j = 0; j < columns; j++)
                    {
                        if (tbX >= colBd[j])
                        {
                            tileX = j;
                        }
                    }

                    for (Ipp32u j = 0; j < rows; j++)
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

                for (Ipp32u i = 0; i < WidthInLCU * HeightInLCU; i++)
                {
                    Ipp32u CtbAddrRStoTS = pps.m_CtbAddrRStoTS[i];
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
                for (Ipp32u i = 0; i < WidthInLCU * HeightInLCU + 1; i++)
                {
                    pps.m_CtbAddrTStoRS[i] = i;
                    pps.m_CtbAddrRStoTS[i] = i;
                }
                memset(pps.m_TileIdx, 0, sizeof(Ipp32u) * WidthInLCU * HeightInLCU);
            }

            Ipp32s numberOfTiles = pps.tiles_enabled_flag ? pps.getNumTiles() : 0;

            pps.tilesInfo.resize(numberOfTiles);

            if (!numberOfTiles)
            {
                pps.tilesInfo.resize(1);
                pps.tilesInfo[0].firstCUAddr = 0;
                pps.tilesInfo[0].endCUAddr = WidthInLCU*HeightInLCU;
            }

            // Initialize tiles coordinates
            for (Ipp32s i = 0; i < numberOfTiles; i++)
            {
                Ipp32s tileY = i / pps.num_tile_columns;
                Ipp32s tileX = i % pps.num_tile_columns;

                Ipp32s startY = 0;
                Ipp32s startX = 0;

                for (Ipp32s j = 0; j < tileX; j++)
                {
                    startX += pps.column_width[j];
                }

                for (Ipp32s j = 0; j < tileY; j++)
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

    //MemoryPiece memCopy;
    //memCopy.SetData(nalUnit);

    pSlice->SetDecryptParameters(pDecryptParams);
    pSlice->m_source.SetTime(pDecryptParams->GetTime());
    //pSlice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

    //notifier0<MemoryPiece> memory_leak_preventing(&pSlice->m_source, &MemoryPiece::Release);

    //std::vector<Ipp32u> removed_offsets(0);
    //SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    //swapper->SwapMemory(&pSlice->m_source, &memCopy, &removed_offsets);

    Ipp32s pps_pid = pSlice->RetrievePicParamSetNumber();
    if (pps_pid == -1)
    {
        return 0;
    }

    pSlice->SetPicParam(m_Headers.m_PicParams.GetHeader(pps_pid));
    if (!pSlice->GetPicParam())
    {
        return 0;
    }

    Ipp32s seq_parameter_set_id = pSlice->GetPicParam()->pps_seq_parameter_set_id;

    pSlice->SetSeqParam(m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id));
    if (!pSlice->GetSeqParam())
    {
        return 0;
    }

    // do not need vps
    //H265VideoParamSet * vps = m_Headers.m_VideoParams.GetHeader(pSlice->GetSeqParam()->sps_video_parameter_set_id);

    m_Headers.m_SeqParams.SetCurrentID(pSlice->GetPicParam()->pps_seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pSlice->GetPicParam()->pps_pic_parameter_set_id);

    pSlice->m_pCurrentFrame = NULL;

    //memory_leak_preventing.ClearNotification();

    if (!pSlice->Reset(&m_pocDecoding))
    {
        return 0;
    }

    if (m_WaitForIDR)
    {
        if (pSlice->GetSliceHeader()->slice_type != I_SLICE)
        {
            return 0;
        }
    }

    ActivateHeaders(const_cast<H265SeqParamSet *>(pSlice->GetSeqParam()), const_cast<H265PicParamSet *>(pSlice->GetPicParam()));

    /*H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();
    Ipp32u currOffset = sliceHdr->m_HeaderBitstreamOffset;
    Ipp32u currOffsetWithEmul = currOffset;

    size_t headersEmuls = 0;
    for (; headersEmuls < removed_offsets.size(); headersEmuls++)
    {
        if (removed_offsets[headersEmuls] < currOffsetWithEmul)
            currOffsetWithEmul++;
        else
            break;
    }

    // Update entry points
    size_t offsets = removed_offsets.size();
    if (pSlice->GetPicParam()->tiles_enabled_flag && pSlice->getTileLocationCount() > 0 && offsets > 0)
    {
        Ipp32u removed_bytes = 0;
        std::vector<Ipp32u>::iterator it = removed_offsets.begin();
        Ipp32u emul_offset = *it;

        for (Ipp32s tile = 0; tile < (Ipp32s)pSlice->getTileLocationCount(); tile++)
        {
            while ((pSlice->m_tileByteLocation[tile] + currOffsetWithEmul >= emul_offset) && removed_bytes < offsets)
            {
                removed_bytes++;
                if (removed_bytes < offsets)
                {
                    it++;
                    emul_offset = *it;
                }
                else
                    break;
            }

            // 1st tile start offset is length of slice header, it should not be corrected because it is
            // not specified in slice header, it is calculated by parsing a modified bitstream instead
            if (0 == tile)
            {
                offsets -= removed_bytes;
                removed_bytes = 0;
            }
            else
                pSlice->m_tileByteLocation[tile] = pSlice->m_tileByteLocation[tile] - removed_bytes;
        }
    }

    for (Ipp32s tile = 0; tile < pSlice->getTileLocationCount(); tile++)
    {
        pSlice->m_tileByteLocation[tile] = pSlice->m_tileByteLocation[tile] + currOffset;
    }*/

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    //for SliceIdx m_SliceIdx m_iNumber
    pSlice->m_WidevineStatusReportNumber = pDecryptParams->ui16StatusReportFeebackNumber;
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

UMC::Status WidevineTaskSupplier::DecryptWidevineHeaders(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    UMC::Status sts = m_pWidevineDecrypter->DecryptFrame(pSource, pDecryptParams);
    if(sts != UMC::UMC_OK)
    {
        return sts;
    }

    return UMC::UMC_OK;
}

UMC::Status WidevineTaskSupplier::AddSource(DecryptParametersWrapper* pDecryptParams)
{
    UMC::Status umcRes = UMC::UMC_OK;

    CompleteDecodedFrames(0);

    if (GetFrameToDisplayInternal(false))
        return UMC::UMC_OK;

    umcRes = AddOneFrame(pDecryptParams); // construct frame

    if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
    {
        ViewItem_H265 &view = *GetView();

        Ipp32s count = 0;
        for (H265DecoderFrame *pFrame = view.pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            count++;
            // frame is being processed. Wait for asynchronous end of operation.
            if (pFrame->isDisposable() || isInDisplayngStage(pFrame) || isAlmostDisposable(pFrame))
            {
                return UMC::UMC_WRN_INFO_NOT_READY;
            }
        }

        if (count < view.pDPB->GetDPBSize())
            return UMC::UMC_WRN_INFO_NOT_READY;

        // some more hard reasons of frame lacking.
        if (!m_pTaskBroker->IsEnoughForStartDecoding(true))
        {
            if (CompleteDecodedFrames(0) == UMC::UMC_OK)
                return UMC::UMC_WRN_INFO_NOT_READY;

            if (GetFrameToDisplayInternal(true))
                return UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            PreventDPBFullness();
            return UMC::UMC_WRN_INFO_NOT_READY;
        }
    }

    return umcRes;
}

UMC::Status WidevineTaskSupplier::AddOneFrame(DecryptParametersWrapper* pDecryptParams)
{
#if defined(MFX_VA)
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
#endif
    UMC::Status umsRes = UMC::UMC_OK;

    if (m_pLastSlice)
        return AddSlice(m_pLastSlice, true);

    umsRes = ParseWidevineVPSSPSPPS(pDecryptParams);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

    umsRes = ParseWidevineSEI(pDecryptParams);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

    H265Slice * pSlice = ParseWidevineSliceHeader(pDecryptParams);
    if (pSlice)
    {
        UMC::Status sts = AddSlice(pSlice, true);
        if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
            return sts;
    }

    return AddSlice(0, true);
}

UMC::Status WidevineTaskSupplier::AddOneFrame(UMC::MediaData * pSource)
{
#if defined(MFX_VA)
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return TaskSupplier_H265::AddOneFrame(pSource);
    }
#endif

    UMC::Status umsRes = UMC::UMC_OK;

    if (m_pLastSlice)
    {
        UMC::Status sts = AddSlice(m_pLastSlice, !pSource);
        if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
            return sts;
    }

    //if (m_checkCRAInsideResetProcess && !pSource)
    //    return UMC::UMC_ERR_FAILED;

    //size_t moveToSpsOffset = m_checkCRAInsideResetProcess ? pSource->GetDataSize() : 0;

    DecryptParametersWrapper decryptParams;
    void* bsDataPointer = pSource ? pSource->GetDataPointer() : 0;

    {
        UMC::Status sts = DecryptWidevineHeaders(pSource, &decryptParams);
        if (sts != UMC::UMC_OK)
            return sts;
    }

    bool decrypted = pSource ? (bsDataPointer != pSource->GetDataPointer()) : false;

    if (!decrypted && pSource)
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            VM_ASSERT(!m_pLastSlice);
            return AddSlice(0, true);
        }

        return UMC::UMC_ERR_SYNC;
    }

    if (!decrypted)
    {
        if (!pSource)
            return AddSlice(0, true);

        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    //do
    //{
    //    UMC::MediaDataEx *nalUnit = m_pNALSplitter->GetNalUnits(pSource);
    //    if (!nalUnit)
    //        break;

    //    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

    //    for (Ipp32s i = 0; i < (Ipp32s)pMediaDataEx->count; i++, pMediaDataEx->index ++)
    //    {
    //        if (m_checkCRAInsideResetProcess)
    //        {
    //            switch ((NalUnitType)pMediaDataEx->values[i])
    //            {
    //            case NAL_UT_CODED_SLICE_RASL_N:
    //            case NAL_UT_CODED_SLICE_RADL_N:
    //            case NAL_UT_CODED_SLICE_TRAIL_R:
    //            case NAL_UT_CODED_SLICE_TRAIL_N:
    //            case NAL_UT_CODED_SLICE_TLA_R:
    //            case NAL_UT_CODED_SLICE_TSA_N:
    //            case NAL_UT_CODED_SLICE_STSA_R:
    //            case NAL_UT_CODED_SLICE_STSA_N:
    //            case NAL_UT_CODED_SLICE_BLA_W_LP:
    //            case NAL_UT_CODED_SLICE_BLA_W_RADL:
    //            case NAL_UT_CODED_SLICE_BLA_N_LP:
    //            case NAL_UT_CODED_SLICE_IDR_W_RADL:
    //            case NAL_UT_CODED_SLICE_IDR_N_LP:
    //            case NAL_UT_CODED_SLICE_CRA:
    //            case NAL_UT_CODED_SLICE_RADL_R:
    //            case NAL_UT_CODED_SLICE_RASL_R:
    //                {
    //                    H265Slice * pSlice = m_ObjHeap.AllocateObject<H265Slice>();
    //                    pSlice->IncrementReference();

    //                    notifier0<H265Slice> memory_leak_preventing_slice(pSlice, &H265Slice::DecrementReference);
    //                    nalUnit->SetDataSize(100); // is enough for retrive

    //                    MemoryPiece memCopy;
    //                    memCopy.SetData(nalUnit);

    //                    pSlice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

    //                    notifier0<MemoryPiece> memory_leak_preventing(&pSlice->m_source, &MemoryPiece::Release);
    //                    SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    //                    swapper->SwapMemory(&pSlice->m_source, &memCopy, 0);

    //                    Ipp32s pps_pid = pSlice->RetrievePicParamSetNumber();
    //                    if (pps_pid != -1)
    //                        CheckCRAOrBLA(pSlice);

    //                    m_checkCRAInsideResetProcess = false;
    //                    pSource->MoveDataPointer(Ipp32s(pSource->GetDataSize() - moveToSpsOffset));
    //                    m_pNALSplitter->Reset();
    //                    return UMC::UMC_NTF_NEW_RESOLUTION;
    //                }
    //                break;

    //            case NAL_UT_VPS:
    //            case NAL_UT_SPS:
    //            case NAL_UT_PPS:
    //                DecodeHeaders(nalUnit);
    //                break;

    //            default:
    //                break;
    //            };

    //            continue;
    //        }
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
            UMC::Status sts = AddSlice(pSlice, !pSource);
            if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                return sts;
        }
    }
    //        switch ((NalUnitType)pMediaDataEx->values[i])
    //        {
    //        case NAL_UT_CODED_SLICE_RASL_N:
    //        case NAL_UT_CODED_SLICE_RADL_N:
    //        case NAL_UT_CODED_SLICE_TRAIL_R:
    //        case NAL_UT_CODED_SLICE_TRAIL_N:
    //        case NAL_UT_CODED_SLICE_TLA_R:
    //        case NAL_UT_CODED_SLICE_TSA_N:
    //        case NAL_UT_CODED_SLICE_STSA_R:
    //        case NAL_UT_CODED_SLICE_STSA_N:
    //        case NAL_UT_CODED_SLICE_BLA_W_LP:
    //        case NAL_UT_CODED_SLICE_BLA_W_RADL:
    //        case NAL_UT_CODED_SLICE_BLA_N_LP:
    //        case NAL_UT_CODED_SLICE_IDR_W_RADL:
    //        case NAL_UT_CODED_SLICE_IDR_N_LP:
    //        case NAL_UT_CODED_SLICE_CRA:
    //        case NAL_UT_CODED_SLICE_RADL_R:
    //        case NAL_UT_CODED_SLICE_RASL_R:
    //            if(H265Slice *pSlice = DecodeSliceHeader(nalUnit))
    //            {
    //                UMC::Status sts = AddSlice(pSlice, !pSource);
    //                if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
    //                    return sts;
    //            }
    //            break;

    //        case NAL_UT_SEI:
    //            DecodeSEI(nalUnit);
    //            break;

    //        case NAL_UT_SEI_SUFFIX:
    //            break;

    //        case NAL_UT_VPS:
    //        case NAL_UT_SPS:
    //        case NAL_UT_PPS:
    //            {
    //                UMC::Status umsRes = DecodeHeaders(nalUnit);
    //                if (umsRes != UMC::UMC_OK)
    //                {
    //                    if (umsRes == UMC::UMC_NTF_NEW_RESOLUTION)
    //                    {
    //                        Ipp32s nalIndex = pMediaDataEx->index;
    //                        Ipp32s size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

    //                        m_checkCRAInsideResetProcess = true;

    //                        if (AddSlice(0, !pSource) == UMC::UMC_OK)
    //                        {
    //                            pSource->MoveDataPointer(- size - 3);
    //                            return UMC::UMC_OK;
    //                        }
    //                        moveToSpsOffset = pSource->GetDataSize() + size + 3;
    //                        continue;
    //                    }

    //                    return umsRes;
    //                }
    //            }
    //            break;

    //        case NAL_UT_AU_DELIMITER:
    //            if (AddSlice(0, !pSource) == UMC::UMC_OK)
    //                return UMC::UMC_OK;
    //            break;

    //        case NAL_UT_EOS:
    //        case NAL_UT_EOB:
    //            m_WaitForIDR = true;
    //            AddSlice(0, !pSource);
    //            m_RA_POC = 0;
    //            m_IRAPType = NAL_UT_INVALID;
    //            GetView()->pDPB->IncreaseRefPicListResetCount(0); // for flushing DPB
    //            NoRaslOutputFlag = 1;
    //            return UMC::UMC_OK;
    //            break;

    //        default:
    //            break;
    //        };
    //    }

    //} while ((pSource) && (MINIMAL_DATA_SIZE_H265 < pSource->GetDataSize()));

    //if (m_checkCRAInsideResetProcess)
    //{
    //    pSource->MoveDataPointer(Ipp32s(pSource->GetDataSize() - moveToSpsOffset));
    //    m_pNALSplitter->Reset();
    //}

    if (!pSource)
    {
        return AddSlice(0, true);
    }
    else
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            VM_ASSERT(!m_pLastSlice);
            return AddSlice(0, true);
        }
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

void WidevineTaskSupplier::CompleteFrame(H265DecoderFrame * pFrame)
{
    VATaskSupplier::CompleteFrame(pFrame);

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->ReleaseForNewBitstream();
    }
#endif

    return;
}


} // namespace UMC_HEVC_DECODER

#endif // MFX_PROTECTED_FEATURE_DISABLE
#endif // UMC_ENABLE_H265_VIDEO_DECODER
