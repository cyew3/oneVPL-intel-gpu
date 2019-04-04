// Copyright (c) 2018-2019 Intel Corporation
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

#include "umc_defs.h"

#if defined MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "umc_mpeg2_defs.h"
#include "umc_mpeg2_slice.h"
#include "umc_mpeg2_frame.h"
#include "mfx_utils.h"
#if defined(_WIN32) || defined(_WIN64)
#include "umc_mpeg2_dxva_packer.h"
#else
#include "umc_mpeg2_va_packer.h"
#endif


namespace UMC_MPEG2_DECODER
{

#if defined(_WIN32) || defined(_WIN64)

    Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
    {
        return new PackerDXVA(va);
    }

    Packer::Packer(UMC::VideoAccelerator * va)
        : m_va(va)
    {}

    Packer::~Packer()
    {}



/****************************************************************************************************/
// Windows DXVA packer implementation
/****************************************************************************************************/

    PackerDXVA::PackerDXVA(UMC::VideoAccelerator * va)
        : Packer(va)
    {}

    void PackerDXVA::BeginFrame() {}
    void PackerDXVA::EndFrame() {}

    // Pack picture
    void PackerDXVA::PackAU(MPEG2DecoderFrame const& frame, uint8_t fieldIndex)
    {
        size_t sliceCount = frame.GetAU(fieldIndex)->GetSliceCount();

        if (!sliceCount)
            return;

        const MPEG2DecoderFrameInfo & frameInfo = *frame.GetAU(fieldIndex);
        PackPicParams(frame, fieldIndex);

        PackQmatrix(frameInfo);

        CreateSliceDataBuffer(frameInfo);
        CreateSliceParamBuffer(frameInfo);

        PackSliceParams(*frame.GetAU(fieldIndex));

        auto s = m_va->Execute();
        if(s != UMC::UMC_OK)
            throw mpeg2_exception(s);
    }

    // Pack picture parameters
    void PackerDXVA::PackPicParams(const MPEG2DecoderFrame & frame, uint8_t fieldIndex)
    {
        const MPEG2DecoderFrameInfo & frameInfo = *frame.GetAU(fieldIndex);
        const auto slice  = frameInfo.GetSlice(0);
        const auto seq    = slice->GetSeqHeader();
        const auto pic    = slice->GetPicHeader();
        const auto picExt = slice->GetPicExtHeader();

        UMC::UMCVACompBuffer *pPicParamBuf;
        auto picParam = (DXVA_PictureParameters*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &pPicParamBuf); // Allocate buffer
        if (!picParam)
            throw mpeg2_exception(UMC::UMC_ERR_FAILED);

        memset(picParam, 0, sizeof(DXVA_PictureParameters));

        const auto refPic0 = frameInfo.GetForwardRefPic();
        const auto refPic1 = frameInfo.GetBackwardRefPic();

        
        int32_t pict_type       = pic.picture_coding_type;
        int32_t width_in_MBs    = (int32_t)(mfx::align2_value<uint32_t>(seq.horizontal_size_value, 16) / 16);
        int32_t height_in_MBs   = (int32_t)(mfx::align2_value<uint32_t>(seq.vertical_size_value,   16) / 16);
        int32_t numMB           = width_in_MBs * height_in_MBs;
        int32_t pict_struct     = picExt.picture_structure;
        int32_t secondfield     = (pict_struct != FRM_PICTURE) && (fieldIndex == 1);

        pPicParamBuf->SetDataSize((int32_t)(sizeof(DXVA_PictureParameters)));
        pPicParamBuf->FirstMb = 0;
        pPicParamBuf->NumOfMB = (picExt.picture_structure == FRM_PICTURE) ? numMB : numMB / 2;

        if (MPEG2_P_PICTURE == pic.picture_coding_type && refPic0)
        {
            picParam->wForwardRefPictureIndex   = (WORD)m_va->GetSurfaceID(refPic0->GetMemID());
            picParam->wBackwardRefPictureIndex  = 0xffff;
        }
        else if (MPEG2_B_PICTURE == pic.picture_coding_type && refPic0 && refPic1)
        {
            picParam->wForwardRefPictureIndex   = (WORD)m_va->GetSurfaceID(refPic0->GetMemID());
            picParam->wBackwardRefPictureIndex  = (WORD)m_va->GetSurfaceID(refPic1->GetMemID());
        }
        else
        {
            picParam->wForwardRefPictureIndex   = 0xffff;
            picParam->wBackwardRefPictureIndex  = 0xffff;
        }

        picParam->wDecodedPictureIndex = (WORD)(frame.GetMemID());

        picParam->bPicDeblocked = 0;
        picParam->wDeblockedPictureIndex = 0;

        picParam->wPicWidthInMBminus1 = (WORD)(width_in_MBs - 1);

        if (pict_struct == FRM_PICTURE)
            picParam->wPicHeightInMBminus1 = (WORD)(height_in_MBs - 1);
        else
            picParam->wPicHeightInMBminus1 = (WORD)(height_in_MBs / 2 - 1);

        picParam->bMacroblockWidthMinus1 = 15;
        picParam->bMacroblockHeightMinus1 = 15;
        picParam->bBlockWidthMinus1 = 7;
        picParam->bBlockHeightMinus1 = 7;
        picParam->bBPPminus1 = 7;
        picParam->bPicStructure = (uint8_t)pict_struct;
        picParam->bSecondField = (uint8_t)secondfield;
        picParam->bPicIntra = (BYTE)((pict_type == MPEG2_I_PICTURE) ? 1 : 0);
        picParam->bPicBackwardPrediction = (BYTE)((pict_type == MPEG2_B_PICTURE) ? 1 : 0);
        picParam->bBidirectionalAveragingMode = 0;
        picParam->bMVprecisionAndChromaRelation = 0;

        picParam->bChromaFormat = 1; // 4:2:0
        picParam->bPicScanFixed = 1;

        picParam->bPicScanMethod = (BYTE)picExt.alternate_scan;
        picParam->bPicReadbackRequests = 0;
        picParam->bRcontrol = 0;
        picParam->bPicSpatialResid8 = 0;
        picParam->bPicOverflowBlocks = 0;
        picParam->bPicExtrapolation = 0;

        picParam->wDeblockedPictureIndex = 0;
        picParam->bPicDeblocked = 0;
        picParam->bPicDeblockConfined = 0;
        picParam->bPic4MVallowed = 0;
        picParam->bPicOBMC = 0;
        picParam->bPicBinPB = 0;
        picParam->bMV_RPS = 0;

        {
            int32_t i;
            int32_t f;
            for (i = 0, picParam->wBitstreamFcodes = 0; i < 4; i++)
            {
                f = picParam->wBitstreamFcodes;
                f |= (picExt.f_code[i] << (12 - 4 * i));
                picParam->wBitstreamFcodes = (WORD)f;
            }
            f = (int16_t)(picExt.intra_dc_precision << 14);
            f |= (pict_struct << 12);
            f |= (picExt.top_field_first << 11);
            f |= (picExt.frame_pred_frame_dct << 10);
            f |= (picExt.concealment_motion_vectors << 9);
            f |= (picExt.q_scale_type << 8);
            f |= (picExt.intra_vlc_format << 7);
            f |= (picExt.alternate_scan << 6);
            f |= (picExt.repeat_first_field << 5);

            f |= (1 << 4); //chroma_420
            f |= (picExt.progressive_frame << 3);
            picParam->wBitstreamPCEelements = (WORD)f;
        }


        picParam->bBitstreamConcealmentNeed = 0;
        picParam->bBitstreamConcealmentMethod = 0;
        picParam->bReservedBits = 0;
    }

    void PackerDXVA::CreateSliceParamBuffer(const MPEG2DecoderFrameInfo & info)
    {
        uint32_t count = (uint32_t)info.GetSliceCount();

        UMC::UMCVACompBuffer *pSliceParamBuf;
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &pSliceParamBuf);
        if (!pSliceParamBuf)
            throw mpeg2_exception(UMC::UMC_ERR_FAILED);

        pSliceParamBuf->SetNumOfItem(count);
        if (count * sizeof(DXVA_SliceInfo) > (uint32_t)pSliceParamBuf->GetBufferSize())
            throw mpeg2_exception(UMC::UMC_ERR_ALLOC);
        else
            pSliceParamBuf->SetDataSize(count * sizeof(DXVA_SliceInfo));
    }

    // Calculate size of slice data and create buffer
    void PackerDXVA::CreateSliceDataBuffer(const MPEG2DecoderFrameInfo & /*info*/)
    {
        UMC::UMCVACompBuffer* compBuf;
        m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBuf); // Allocate buffer
        if (!compBuf)
            throw mpeg2_exception(UMC::UMC_ERR_FAILED);

        compBuf->SetDataSize(0);
    }

    // Pack slice parameters
    void PackerDXVA::PackSliceParams(const MPEG2DecoderFrameInfo & frameInfo)
    {
        size_t sliceCount = frameInfo.GetSliceCount();

        for (size_t sliceNum = 0; sliceNum < sliceCount; sliceNum++)
        {
            const auto slice        = frameInfo.GetSlice(sliceNum);
            const auto sliceHeader  = slice->GetSliceHeader();
            const auto seqHeader    = slice->GetSeqHeader();
            const auto seqExtHeader = slice->GetSeqExtHeader();
            const auto pic          = slice->GetPicHeader();
            const auto picExt       = slice->GetPicExtHeader();


            UMC::UMCVACompBuffer* compBuf;
            auto sliceParams = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBuf);
            if (!sliceParams)
                throw mpeg2_exception(UMC::UMC_ERR_FAILED);

            if (m_va->IsLongSliceControl())
            {
                sliceParams += sliceNum;
                memset(sliceParams, 0, sizeof(DXVA_SliceInfo));
            }
            else
            {
                sliceParams = (DXVA_SliceInfo*)(sliceParams + sliceNum);
                memset(sliceParams, 0, sizeof(DXVA_SliceInfo));
            }

            uint8_t *  rawDataPtr = nullptr;
            uint32_t  rawDataSize = 0;

            slice->GetBitStream().GetOrg(rawDataPtr, rawDataSize);
            sliceParams->dwSliceBitsInBuffer = (rawDataSize + prefix_size) * 8;

            int32_t width_in_MBs = (int32_t)(mfx::align2_value<uint32_t>(seqHeader.horizontal_size_value, 16) / 16);
            int32_t height_in_MBs = (int32_t)(mfx::align2_value<uint32_t>(seqHeader.vertical_size_value, 16) / 16);
            int32_t numMB = width_in_MBs * height_in_MBs;

            compBuf->NumOfMB = (picExt.picture_structure == FRM_PICTURE) ? numMB : numMB / 2;

            auto sliceDataBuf = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBuf);
            if (!sliceDataBuf)
                throw mpeg2_exception(UMC::UMC_ERR_FAILED);

            sliceParams->dwSliceDataLocation = (DWORD)compBuf->GetDataSize();
            sliceDataBuf += sliceParams->dwSliceDataLocation;

            std::copy(start_code_prefix, start_code_prefix + prefix_size, sliceDataBuf);
            std::copy(rawDataPtr, rawDataPtr + rawDataSize, sliceDataBuf + prefix_size);
            compBuf->SetDataSize(sliceParams->dwSliceDataLocation + (sliceParams->dwSliceBitsInBuffer / 8));

            if (!m_va->IsLongSliceControl())
                return;

            sliceParams->wMBbitOffset = (WORD)sliceHeader.mbOffset + prefix_size * 8;
            sliceParams->wHorizontalPosition = (WORD)sliceHeader.macroblockAddressIncrement;
            sliceParams->wVerticalPosition = (WORD)sliceHeader.slice_vertical_position - 1;
            sliceParams->wQuantizerScaleCode = (WORD)sliceHeader.quantiser_scale_code;
            sliceParams->wNumberMBsInSlice = (WORD)(width_in_MBs - sliceParams->wHorizontalPosition);

            //need to comment this section (and others)
            if (sliceNum > 0 && sliceParams->wHorizontalPosition > 0)
            {
                auto prevSliceParams = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER);
                if (!prevSliceParams)
                    throw mpeg2_exception(UMC::UMC_ERR_FAILED);
                
                if (m_va->IsLongSliceControl())
                    prevSliceParams += sliceNum - 1; //take previous
                else
                    prevSliceParams = (DXVA_SliceInfo*)(sliceParams + sliceNum - 1);

                prevSliceParams->wNumberMBsInSlice = (WORD)(sliceHeader.macroblockAddressIncrement - prevSliceParams->wHorizontalPosition);
            }
        }

        return;
    }

    static const uint8_t default_intra_quantizer_matrix[64] =
    {
         8, 16, 16, 19, 16, 19, 22, 22,
        22, 22, 22, 22, 26, 24, 26, 27,
        27, 27, 26, 26, 26, 26, 27, 27,
        27, 29, 29, 29, 34, 34, 34, 29,
        29, 29, 27, 27, 29, 29, 32, 32,
        34, 34, 37, 38, 37, 35, 35, 34,
        35, 38, 38, 40, 40, 40, 48, 48,
        46, 46, 56, 56, 58, 69, 69, 83
    };

    static const uint8_t default_non_intra_quantizer_matrix[64] =
    {
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16
    };

    // Pack matrix parameters
    void PackerDXVA::PackQmatrix(const MPEG2DecoderFrameInfo & info)
    {
        UMC::UMCVACompBuffer *quantBuf;
        auto qmatrix = (DXVA_QmatrixData*)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &quantBuf, sizeof(DXVA_QmatrixData)); // Allocate buffer
        if (!qmatrix)
            throw mpeg2_exception(UMC::UMC_ERR_FAILED);

        const auto slice    = info.GetSlice(0);
        const auto seq      = slice->GetSeqHeader();
        const auto customQM = slice->GetQMatrix();

        quantBuf->SetDataSize(sizeof(DXVA_QmatrixData));

        // intra_quantizer_matrix is bNewQmatrix[0]
        qmatrix->bNewQmatrix[0] = 1;
        const uint8_t * intra_quantizer_matrix = customQM && customQM->load_intra_quantiser_matrix ?
                                                    customQM->intra_quantiser_matrix :
                                                    (seq.load_intra_quantiser_matrix ? seq.intra_quantiser_matrix : default_intra_quantizer_matrix);
        std::copy(intra_quantizer_matrix, intra_quantizer_matrix + 64, qmatrix->Qmatrix[0]);

        // chroma_intra_quantiser_matrix is bNewQmatrix[2]
        qmatrix->bNewQmatrix[2] = 1;
        const uint8_t * chroma_intra_quantiser_matrix = customQM && customQM->load_chroma_intra_quantiser_matrix ?
                                                            customQM->chroma_intra_quantiser_matrix :
                                                            (seq.load_intra_quantiser_matrix ? seq.intra_quantiser_matrix : default_intra_quantizer_matrix);
        std::copy(chroma_intra_quantiser_matrix, chroma_intra_quantiser_matrix + 64, qmatrix->Qmatrix[2]);

        // non_intra_quantiser_matrix is bNewQmatrix[1]
        qmatrix->bNewQmatrix[1] = 1;
        const uint8_t * non_intra_quantiser_matrix = customQM && customQM->load_non_intra_quantiser_matrix ?
                                                        customQM->non_intra_quantiser_matrix :
                                                        (seq.load_non_intra_quantiser_matrix ? seq.non_intra_quantiser_matrix : default_non_intra_quantizer_matrix);
        std::copy(non_intra_quantiser_matrix, non_intra_quantiser_matrix + 64, qmatrix->Qmatrix[1]);

        // chroma_non_intra_quantiser_matrix is bNewQmatrix[3]
        qmatrix->bNewQmatrix[3] = 1;
        const uint8_t * chroma_non_intra_quantiser_matrix = customQM && customQM->load_chroma_non_intra_quantiser_matrix ?
                                                                customQM->chroma_non_intra_quantiser_matrix :
                                                                (seq.load_non_intra_quantiser_matrix ? seq.non_intra_quantiser_matrix : default_non_intra_quantizer_matrix);
        std::copy(chroma_non_intra_quantiser_matrix, chroma_non_intra_quantiser_matrix + 64, qmatrix->Qmatrix[3]);
    }
#endif // defined(_WIN32) || defined(_WIN64)
}

#endif // MFX_ENABLE_MPEG2_VIDEO_DECODE
