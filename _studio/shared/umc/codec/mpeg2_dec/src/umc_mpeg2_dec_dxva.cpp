/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2013 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_hw.h"
//#define OTLAD
#ifdef OTLAD
#include <stdio.h>

    #define MEDIASDK_ROOT "/home/locusr/mfx_linux/"
    const char* fname_otladka   = MEDIASDK_ROOT"va_otl.txt";
    const char* fname_matr      = MEDIASDK_ROOT"va_matr.txt";
    const char* fname_pic       = MEDIASDK_ROOT"va_pic.txt";
    const char* fname_slice     = MEDIASDK_ROOT"va_slice.txt";
    #undef MEDIASDK_ROOT

    FILE *otl = fopen(fname_otladka, "w");
/*extern */int frame_count = 0;
/*extern*/ int slice_count = 0;
#endif

#pragma warning(disable: 4244)

using namespace UMC;


#ifdef UMC_VA_DXVA
extern Ipp32u DistToNextSlice(mfxEncryptedData *encryptedData, PAVP_COUNTER_TYPE counterMode);
#endif
///////////////////////////////////////////////////////////////////////////
// PackVA class

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

bool PackVA::SetVideoAccelerator(VideoAccelerator * va)
{
    m_va = va;
    if (!m_va)
    {
        va_mode = UNKNOWN;
        return true;
    }
    if ((m_va->m_Profile & VA_CODEC) != VA_MPEG2)
    {
        return false;
    }
    va_mode = m_va->m_Profile;
    IsProtectedBS = (m_va->GetProtectedVA() != NULL);

    return true;
}

#endif

#ifdef UMC_VA_DXVA

PackVA::~PackVA(void){}

Status PackVA::InitBuffers(int /*size_bs*/, int /*size_sl*/)
{
      totalNumCoef = 0;

      if (m_SliceNum == 0 && m_va->GetProtectedVA())
      {
          bs = m_va->GetProtectedVA()->GetBitstream();
          curr_bs_encryptedData = bs->EncryptedData;
          curr_encryptedData = curr_bs_encryptedData;
          is_bs_aligned_16 = true;
          add_to_slice_start = 0;
      }

      // request picture parameters memory
      vpPictureParam = (DXVA_PictureParameters*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER);

      if (NULL == vpPictureParam)
      {
          return UMC_ERR_FAILED;
      }

      UMCVACompBuffer *CompBuf = NULL;

      // request inv quant matrix memory
      pQmatrixData = (DXVA_QmatrixData*)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER);

      if (NULL == pQmatrixData)
      {
          return UMC_ERR_FAILED;
      }

      // request slice infromation buffer
      pSliceInfoBuffer = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &CompBuf);
      
      if (NULL == CompBuf)
      {
          return UMC_ERR_FAILED;
      }

      slice_size_getting = CompBuf->GetBufferSize();

      CompBuf = NULL;

      // request bitstream buffer
      pBitsreamData = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

      if (NULL == CompBuf)
      {
          return UMC_ERR_FAILED;
      }

      if (NULL != m_va->GetProtectedVA() &&
          IS_PROTECTION_GPUCP_ANY(m_va->GetProtectedVA()->GetProtected()) &&
          curr_bs_encryptedData != NULL)
      {
          CompBuf->SetPVPState(&curr_bs_encryptedData->CipherCounter, sizeof(curr_bs_encryptedData->CipherCounter));
      }
      else
      {
          CompBuf->SetPVPState(NULL, 0);
      }

      bs_size_getting = CompBuf->GetBufferSize();

      pSliceInfo = pSliceInfoBuffer;

      return UMC_OK;
}

Status
PackVA::SetBufferSize(
    Ipp32s          numMB,
    MPEG2FrameType  picture_coding_type,
    int             size_bs,
    int             size_sl)
{
    (size_bs);
    (size_sl);

    UMCVACompBuffer* CompBuf;
    try
    {
        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf);
        if (NULL == CompBuf)
        {
                return UMC_ERR_FAILED;
        }
        CompBuf->SetDataSize((Ipp32s) (sizeof (DXVA_PictureParameters)));
        CompBuf->FirstMb = 0;
        CompBuf->NumOfMB = numMB;

        if(va_mode == VA_IT_W)
        {
            m_va->GetCompBuffer(DXVA_MACROBLOCK_CONTROL_BUFFER, &CompBuf);
            Ipp32s size = (Ipp32s)((picture_coding_type == MPEG2_I_PICTURE) ?
                numMB*sizeof(DXVA_MBctrl_I_OffHostIDCT_1) :
                numMB*sizeof(DXVA_MBctrl_P_OffHostIDCT_1));
            CompBuf->SetDataSize(size);

            m_va->GetCompBuffer(DXVA_RESIDUAL_DIFFERENCE_BUFFER, &CompBuf);
            CompBuf->SetDataSize((Ipp32s) (totalNumCoef*sizeof(DXVA_TCoefSingle)));
        }
        else if(va_mode == VA_VLD_W)
        {
            CompBuf = NULL;
            m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &CompBuf);
            if (NULL == CompBuf)
            {
                return UMC_ERR_FAILED;
            }
            CompBuf->SetDataSize((Ipp32s) sizeof(DXVA_QmatrixData));
            CompBuf->FirstMb = 0;
            CompBuf->NumOfMB = numMB;

            CompBuf = NULL;
            m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &CompBuf);
            if (NULL == CompBuf)
            {
                    return UMC_ERR_FAILED;
            }
            CompBuf->SetDataSize((Ipp32s) ((Ipp8u*)pSliceInfo - (Ipp8u*)pSliceInfoBuffer));
            CompBuf->FirstMb = 0;
            CompBuf->NumOfMB = numMB;

            CompBuf = NULL;
            m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
            //CompBuf->SetDataSize((Ipp32s) (pSliceInfo[-1].dwSliceDataLocation +
                
            if (NULL == CompBuf)
            {
                    return UMC_ERR_FAILED;
            }
                
            CompBuf->SetDataSize((bs_size > bs_size_getting) ? bs_size_getting : bs_size);
            CompBuf->FirstMb = 0;
            CompBuf->NumOfMB = numMB;

            // CompBuf->SetNumOfItem((Ipp32s)(pSliceInfo - pSliceInfoBuffer));
        }
    }
    catch(...)
    {
        return UMC_ERR_NOT_ENOUGH_BUFFER;
    }

    return UMC_OK;
}

Status
PackVA::SaveVLDParameters(
    sSequenceHeader*    sequenceHeader,
    sPictureHeader*     PictureHeader,
    Ipp8u*              bs_start_ptr,
    sFrameBuffer*       frame_buffer,
    Ipp32s              task_num,
    Ipp32s)
{
  if(va_mode == VA_NO) return UMC_OK;

#ifdef OTLAD
  slice_count = 0;
  if(otl)fprintf(otl,"frame_count = %d\n",frame_count);
#endif

  if(va_mode == VA_VLD_W)
  {
        mfxU16 prot = 0;
        if (m_va->GetProtectedVA())
        {
            prot = m_va->GetProtectedVA()->GetProtected();
            Ipp32s i;

            Ipp32s NumSlices = (Ipp32s) (pSliceInfo - pSliceInfoBuffer);
            mfxEncryptedData *encryptedData = curr_bs_encryptedData;

           // if (m_SliceNum == 0)
            if (MFX_PROTECTION_PAVP == prot)
            {
                HRESULT hr;
                DXVA2_DecodeExtensionData  extensionData = {0};
                DXVA_EncryptProtocolHeader extensionOutput = {0};

                extensionData.Function = 0;
                extensionData.pPrivateOutputData = &extensionOutput;
                extensionData.PrivateOutputDataSize = sizeof(extensionOutput);
                if (m_va->m_HWPlatform == VA_HW_LAKE)
                {
                    DXVA_Intel_Pavp_Protocol  extensionInput = {0};

                    extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                    if(encryptedData)
                    {
                        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = m_va->GetProtectedVA()->GetEncryptionGUID();
                        extensionInput.dwBufferSize = bs_size;
                        extensionInput.dwAesCounter = LITTLE_ENDIAN_SWAP32((DWORD)(curr_bs_encryptedData->CipherCounter.Count >> 32));
                    }
                    else
                    {
                        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = DXVA_NoEncrypt;
                    }
                    extensionData.pPrivateInputData = &extensionInput;
                    extensionData.PrivateInputDataSize = sizeof(extensionInput);
                    hr = m_va->ExecuteExtensionBuffer(&extensionData);
                }
                else
                {
                    DXVA_Intel_Pavp_Protocol2  extensionInput = {0};

                    extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                    if(encryptedData)
                    {
                        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = m_va->GetProtectedVA()->GetEncryptionGUID();
                        extensionInput.dwBufferSize = bs_size;
                        memcpy_s(extensionInput.dwAesCounter, sizeof(curr_bs_encryptedData->CipherCounter),
                            &curr_bs_encryptedData->CipherCounter, sizeof(curr_bs_encryptedData->CipherCounter));
                    }
                    else
                    {
                        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = DXVA_NoEncrypt;
                    }
                    extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
                    extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();
                    extensionData.pPrivateInputData = &extensionInput;
                    extensionData.PrivateInputDataSize = sizeof(extensionInput);
                    hr = m_va->ExecuteExtensionBuffer(&extensionData);
                }

                if (FAILED(hr) ||
                    (encryptedData && extensionOutput.guidEncryptProtocol != m_va->GetProtectedVA()->GetEncryptionGUID()) ||
                    //This is workaround for 15.31 driver that returns GUID_NULL instead DXVA_NoEncrypt
                    /*(!encryptedData && extensionOutput.guidEncryptProtocol != DXVA_NoEncrypt) ||*/
                    extensionOutput.dwFunction != 0xffff0801)
                {
                    return UMC_ERR_DEVICE_FAILED;
                }
            }

            if(encryptedData)
            {
                Ipp8u* ptr = (Ipp8u*)pBitsreamData;
                Ipp8u* pBuf = ptr;

                DXVA_SliceInfo * p = pSliceInfoBuffer;

                p[0].dwSliceDataLocation = add_to_slice_start + encryptedData->DataOffset;
                //bs_size += add_to_slice_start;

                if(add_to_slice_start)
                {
                    ippsCopy_8u(add_bytes,ptr, add_to_slice_start);
                    ptr += add_to_slice_start;
                    pBuf=ptr;
                }
                Ipp32u alignedSize = 0;
                Ipp32u diff = 0;
                for (i = 0; i < NumSlices; i++)
                {
                    alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                    if(alignedSize & 0xf)
                        alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));

                    ippsCopy_8u(encryptedData->Data,pBuf, alignedSize);

                    diff = (Ipp32u)(ptr - pBuf);
                    ptr += alignedSize - diff;

                    (unsigned char*)pBuf += DistToNextSlice(encryptedData,
                        (PAVP_COUNTER_TYPE)(m_va->GetProtectedVA())->GetCounterMode());

                    if(i < NumSlices-1)
                    {
                        diff = (Ipp32u)(ptr - pBuf);
                        p[i+1].dwSliceDataLocation = p[i].dwSliceDataLocation +
                            (alignedSize - encryptedData->DataOffset)+
                            encryptedData->Next->DataOffset - diff;
                    }
                    encryptedData = encryptedData->Next;
                    if (NULL == encryptedData)
                        break;
                }
                // there is not enough encrypted data
                if (i < NumSlices - 1)
                    return UMC_ERR_DEVICE_FAILED;

                m_SliceNum += NumSlices;
                curr_bs_encryptedData = encryptedData;
                curr_encryptedData = curr_bs_encryptedData;
                overlap = 0;
            }
            else // if(encryptedData)
            {
                bs_size = pSliceInfo[-1].dwSliceDataLocation +
                    pSliceInfo[-1].dwSliceBitsInBuffer/8;
                ippsCopy_8u(bs_start_ptr, (Ipp8u*)pBitsreamData, bs_size);
            }
        }//if (pack_w.m_va->GetProtectedVA())
        else//not protected
        {
          bs_size = pSliceInfo[-1].dwSliceDataLocation +
                        pSliceInfo[-1].dwSliceBitsInBuffer/8;
          ippsCopy_8u(bs_start_ptr, (Ipp8u*)pBitsreamData, bs_size);
        }

      Ipp32s from, to;
      for(from = 0, to = 0; from < 4; from++)
      {
        pQmatrixData->bNewQmatrix[from] = QmatrixData.bNewQmatrix[from];
        if(QmatrixData.bNewQmatrix[from])
        {
          ippsCopy_8u((Ipp8u*)QmatrixData.Qmatrix[from], (Ipp8u*)pQmatrixData->Qmatrix[to],
            sizeof(pQmatrixData->Qmatrix[0]));
          to++;
        }
      }
  }

  if (vpPictureParam) {
    DXVA_PictureParameters *pPictureParam = (DXVA_PictureParameters*)vpPictureParam;
    Ipp32s pict_type = PictureHeader->picture_coding_type;
    Ipp32s width_in_MBs = sequenceHeader->mb_width[task_num];
    Ipp32s height_in_MBs = sequenceHeader->mb_height[task_num];
    Ipp32s pict_struct = PictureHeader->picture_structure;
    Ipp32s secondfield = (pict_struct != FRAME_PICTURE) && (field_buffer_index == 1);

    Ipp32s f_ref_pic = -1;
    if(pict_type == MPEG2_P_PICTURE)
    {
        if (frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index < 0)
        {
            f_ref_pic = 0;
        }
        else if(frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index].va_index < 0)
        {
            f_ref_pic = frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].va_index;
        }
        else
        {
            f_ref_pic = frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index].va_index;
        }
    }
    else if(pict_type == MPEG2_B_PICTURE)
    {
        Ipp32s idx = frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index; 

        if (idx < 0)
        {
            f_ref_pic = 0;
        }
        else
            f_ref_pic = frame_buffer->frame_p_c_n[idx].va_index;
    }

    memset(pPictureParam, 0, sizeof(*pPictureParam));

    pPictureParam->wDecodedPictureIndex = (WORD)(frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].va_index);
    pPictureParam->wForwardRefPictureIndex = (WORD)((pict_type == MPEG2_I_PICTURE) ? 0xffff : f_ref_pic);
    pPictureParam->wBackwardRefPictureIndex = (WORD)((pict_type == MPEG2_B_PICTURE) ? frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].next_index].va_index : 0xffff);
    pPictureParam->bPicDeblocked = 0;
    pPictureParam->wDeblockedPictureIndex = 0;

    pPictureParam->wPicWidthInMBminus1 = (WORD)(width_in_MBs - 1);

    if(pict_struct == FRAME_PICTURE)
        pPictureParam->wPicHeightInMBminus1 = (WORD)(height_in_MBs - 1);
    else
        pPictureParam->wPicHeightInMBminus1 = (WORD)(height_in_MBs/2 - 1);

    pPictureParam->bMacroblockWidthMinus1 = 15;
    pPictureParam->bMacroblockHeightMinus1 = 15;
    pPictureParam->bBlockWidthMinus1 = 7;
    pPictureParam->bBlockHeightMinus1 = 7;
    pPictureParam->bBPPminus1 = 7;
    pPictureParam->bPicStructure = (Ipp8u)pict_struct;
    pPictureParam->bSecondField = (Ipp8u)secondfield;
    pPictureParam->bPicIntra = (BYTE)((pict_type == MPEG2_I_PICTURE) ? 1 : 0);
    pPictureParam->bPicBackwardPrediction = (BYTE)((pict_type == MPEG2_B_PICTURE) ? 1 : 0);
    pPictureParam->bBidirectionalAveragingMode = 0;
    pPictureParam->bMVprecisionAndChromaRelation = 0;

    pPictureParam->bChromaFormat = 1; // 4:2:0
    pPictureParam->bPicScanFixed = 1;

    pPictureParam->bPicScanMethod = (BYTE)PictureHeader->alternate_scan;
    pPictureParam->bPicReadbackRequests = 0;
    pPictureParam->bRcontrol = 0;
    pPictureParam->bPicSpatialResid8 = 0;
    pPictureParam->bPicOverflowBlocks = 0;
    pPictureParam->bPicExtrapolation = 0;

    pPictureParam->wDeblockedPictureIndex = 0;
    pPictureParam->bPicDeblocked = 0;
    pPictureParam->bPicDeblockConfined = 0;
    pPictureParam->bPic4MVallowed = 0;
    pPictureParam->bPicOBMC = 0;
    pPictureParam->bPicBinPB = 0;
    pPictureParam->bMV_RPS = 0;

    {
      Ipp32s i;
      Ipp32s f;
      for(i=0, pPictureParam->wBitstreamFcodes=0; i<4; i++)
      {
          f = pPictureParam->wBitstreamFcodes;
          f |= (PictureHeader->f_code[i] << (12 - 4*i));
          pPictureParam->wBitstreamFcodes = (WORD)f;
      }
      f = (Ipp16s)(PictureHeader->intra_dc_precision << 14);
      f |= (pict_struct << 12);
      f |= (PictureHeader->top_field_first << 11);
      f |= (PictureHeader->frame_pred_frame_dct << 10);
      f |= (PictureHeader->concealment_motion_vectors << 9);
      f |= (PictureHeader->q_scale_type << 8);
      f |= (PictureHeader->intra_vlc_format << 7);
      f |= (PictureHeader->alternate_scan << 6);
      f |= (PictureHeader->repeat_first_field << 5);

#ifdef ELK
      f |= (1 << 4); //chroma_420
#else
      f |= (PictureHeader[task_num]->progressive_frame << 4); // chroma420 == prog_frame here
#endif
      f |= (PictureHeader->progressive_frame << 3);
      pPictureParam->wBitstreamPCEelements = (WORD)f;
    }


    pPictureParam->bBitstreamConcealmentNeed = 0;
    pPictureParam->bBitstreamConcealmentMethod = 0;
    pPictureParam->bReservedBits = 0;

  }

  return UMC_OK;
}

Status PackVA::GetStatusReport(DXVA_Status_VC1 *pStatusReport)
{
    UMC::Status sts = UMC_OK;
    unsigned int iterNum = 1;

    while (!pStatusReport->StatusReportFeedbackNumber && UMC_OK == sts && iterNum != 0)
    {
        iterNum -= 1;
        
        sts = m_va->ExecuteStatusReportBuffer((void*)pStatusReport, sizeof(DXVA_Status_VC1) * 32);

        if (pStatusReport->StatusReportFeedbackNumber)
        {
            break;
        }
    }

    return sts;
}

#elif defined UMC_VA_LINUX

PackVA::~PackVA(void)
{
    if(pSliceInfoBuffer)
    {
        ippsFree(pSliceInfoBuffer);
        pSliceInfoBuffer = NULL;
    }
}

Status PackVA::InitBuffers(int size_bs, int size_sl)
{
    UMCVACompBuffer* CompBuf;
    if(va_mode != VA_VLD_L)
        return UMC_ERR_UNSUPPORTED;

#if !defined(MFX_NO_EXCEPTIONS)
    try
#endif // #if !defined(MFX_NO_EXCEPTIONS)
    {
        if (NULL == (vpPictureParam = (VAPictureParameterBufferMPEG2*)m_va->GetCompBuffer(VAPictureParameterBufferType, &CompBuf, sizeof (VAPictureParameterBufferMPEG2))))
            return UMC_ERR_ALLOC;

        if (va_mode == VA_VLD_L) // ao: needed?
        {
            if (NULL == (pQmatrixData = (VAIQMatrixBufferMPEG2*)m_va->GetCompBuffer(VAIQMatrixBufferType, &CompBuf, sizeof (VAIQMatrixBufferMPEG2))))
                return UMC_ERR_ALLOC;

            Ipp32s prev_slice_size_getting = slice_size_getting;
            slice_size_getting = sizeof (VASliceParameterBufferMPEG2) * size_sl;

#if 0
            if (NULL == (pSliceInfoBuffer = (VASliceParameterBufferMPEG2*)m_va->GetCompBuffer(VASliceParameterBufferType, &CompBuf, sizeof (VASliceParameterBufferMPEG2)*size_sl)))
                return UMC_ERR_ALLOC;

#else
            if(NULL == pSliceInfoBuffer)
                pSliceInfoBuffer = (VASliceParameterBufferMPEG2*)ippsMalloc_8u(slice_size_getting);
            else if (prev_slice_size_getting < slice_size_getting)
            {
                ippsFree(pSliceInfoBuffer);
                pSliceInfoBuffer = (VASliceParameterBufferMPEG2*)ippsMalloc_8u(slice_size_getting);
            }

            if( NULL == pSliceInfoBuffer)
                return UMC_ERR_ALLOC;
#endif

            memset(pSliceInfoBuffer, 0, slice_size_getting);

            if (NULL == (pBitsreamData = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf, size_bs)))
                return UMC_ERR_ALLOC;

            if (NULL != m_va->GetProtectedVA())
            {
                return UMC_ERR_NOT_IMPLEMENTED;
            }
            else
            {
                CompBuf->SetPVPState(NULL, 0);
            }

            bs_size_getting = CompBuf->GetBufferSize();
            pSliceInfo = pSliceInfoBuffer;
        }
    }
#if !defined(MFX_NO_EXCEPTIONS)
    catch(...)
    {
        return UMC_ERR_NOT_ENOUGH_BUFFER;
    }
#endif //#if !defined(MFX_NO_EXCEPTIONS)
    return UMC_OK;
}

Status
PackVA::SaveVLDParameters(
    sSequenceHeader*    sequenceHeader,
    sPictureHeader*     PictureHeader,
    Ipp8u*              bs_start_ptr,
    sFrameBuffer*       frame_buffer,
    Ipp32s              task_num,
    Ipp32s              source_mb_height)
{
    int i;
    int NumOfItem;

    if(va_mode == VA_NO)
    {
        return UMC_OK;
    }

    if(va_mode == VA_VLD_L)
    {
        Ipp32s size = 0;
        if(pSliceInfoBuffer < pSliceInfo)
            size = pSliceInfo[-1].slice_data_offset + pSliceInfo[-1].slice_data_size;
        ippsCopy_8u(bs_start_ptr, (Ipp8u*)pBitsreamData, size);

        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf);
        Ipp32s buffer_size = CompBuf->GetBufferSize();
        if(buffer_size > size)
            memset(pBitsreamData + size, 0, buffer_size - size);

        pQmatrixData->load_intra_quantiser_matrix = QmatrixData.load_intra_quantiser_matrix;
        pQmatrixData->load_non_intra_quantiser_matrix = QmatrixData.load_non_intra_quantiser_matrix;
        pQmatrixData->load_chroma_intra_quantiser_matrix = QmatrixData.load_chroma_intra_quantiser_matrix;
        pQmatrixData->load_chroma_non_intra_quantiser_matrix = QmatrixData.load_chroma_non_intra_quantiser_matrix;

        ippsCopy_8u((Ipp8u*)QmatrixData.intra_quantiser_matrix, (Ipp8u*)pQmatrixData->intra_quantiser_matrix,
            sizeof(pQmatrixData->intra_quantiser_matrix));
        ippsCopy_8u((Ipp8u*)QmatrixData.non_intra_quantiser_matrix, (Ipp8u*)pQmatrixData->non_intra_quantiser_matrix,
            sizeof(pQmatrixData->non_intra_quantiser_matrix));
        ippsCopy_8u((Ipp8u*)QmatrixData.chroma_intra_quantiser_matrix, (Ipp8u*)pQmatrixData->chroma_intra_quantiser_matrix,
            sizeof(pQmatrixData->chroma_intra_quantiser_matrix));
        ippsCopy_8u((Ipp8u*)QmatrixData.chroma_non_intra_quantiser_matrix, (Ipp8u*)pQmatrixData->chroma_non_intra_quantiser_matrix,
            sizeof(pQmatrixData->chroma_non_intra_quantiser_matrix));

        NumOfItem = (int)(pSliceInfo - pSliceInfoBuffer);

        VASliceParameterBufferMPEG2 *l_pSliceInfoBuffer = (VASliceParameterBufferMPEG2*)m_va->GetCompBuffer(
            VASliceParameterBufferType,
            &CompBuf,
            (Ipp32s) (sizeof (VASliceParameterBufferMPEG2))*NumOfItem);

        ippsCopy_8u((Ipp8u*)pSliceInfoBuffer, (Ipp8u*)l_pSliceInfoBuffer,
            sizeof(VASliceParameterBufferMPEG2) * NumOfItem);

        m_va->GetCompBuffer(VASliceParameterBufferType,&CompBuf);
        CompBuf->SetNumOfItem(NumOfItem);
    }

    if (vpPictureParam)
    {
        Ipp32s pict_type = PictureHeader->picture_coding_type;
        VAPictureParameterBufferMPEG2 *pPictureParam = vpPictureParam;

        pPictureParam->horizontal_size = sequenceHeader->mb_width[task_num] * 16;
        pPictureParam->vertical_size = sequenceHeader->mb_height[task_num] * 16;

        //pPictureParam->forward_reference_picture = m_va->GetSurfaceID(prev_index);//prev_index;
        //pPictureParam->backward_reference_picture = m_va->GetSurfaceID(next_index);//next_index;
        Ipp32s f_ref_pic = VA_INVALID_SURFACE;

        if(pict_type == MPEG2_P_PICTURE)
        {
            if(m_va->GetSurfaceID(frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index].va_index) < 0)
            {
                f_ref_pic = m_va->GetSurfaceID(frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].va_index);
            }
            else
            {
                f_ref_pic = m_va->GetSurfaceID(frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index].va_index);
            }
        }
        else if(pict_type == MPEG2_B_PICTURE)
        {
            f_ref_pic = m_va->GetSurfaceID(frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].prev_index].va_index);
        }

        pPictureParam->forward_reference_picture = (pict_type == MPEG2_I_PICTURE)
            ? VA_INVALID_SURFACE
            : f_ref_pic;
        pPictureParam->backward_reference_picture = (pict_type == MPEG2_B_PICTURE)
            ? m_va->GetSurfaceID(frame_buffer->frame_p_c_n[frame_buffer->frame_p_c_n[frame_buffer->curr_index[task_num]].next_index].va_index)
            : VA_INVALID_SURFACE;

        // NOTE
        // It may occur that after Reset() B-frame is needed only in 1 reference frame for successful decoding,
        // however, we need to provide 2 surfaces.
        if (VA_INVALID_SURFACE == pPictureParam->forward_reference_picture)
            pPictureParam->forward_reference_picture = pPictureParam->backward_reference_picture;

        pPictureParam->picture_coding_type = PictureHeader->picture_coding_type;
        for(i=0, pPictureParam->f_code=0; i<4; i++)
        {
            pPictureParam->f_code |= (PictureHeader->f_code[i] << (12 - 4*i));
        }

        pPictureParam->picture_coding_extension.value = 0;
        pPictureParam->picture_coding_extension.bits.intra_dc_precision = PictureHeader->intra_dc_precision;
        pPictureParam->picture_coding_extension.bits.picture_structure = PictureHeader->picture_structure;
        pPictureParam->picture_coding_extension.bits.top_field_first = PictureHeader->top_field_first;
        pPictureParam->picture_coding_extension.bits.frame_pred_frame_dct = PictureHeader->frame_pred_frame_dct;
        pPictureParam->picture_coding_extension.bits.concealment_motion_vectors = PictureHeader->concealment_motion_vectors;
        pPictureParam->picture_coding_extension.bits.q_scale_type = PictureHeader->q_scale_type;
        pPictureParam->picture_coding_extension.bits.intra_vlc_format = PictureHeader->intra_vlc_format;
        pPictureParam->picture_coding_extension.bits.alternate_scan = PictureHeader->alternate_scan;
        pPictureParam->picture_coding_extension.bits.repeat_first_field = PictureHeader->repeat_first_field;
        pPictureParam->picture_coding_extension.bits.progressive_frame = PictureHeader->progressive_frame;
        pPictureParam->picture_coding_extension.bits.is_first_field = !((PictureHeader->picture_structure != FRAME_PICTURE)
                                            && (field_buffer_index == 1));
    }

#ifdef OTLAD
    {
        slice_count = 0;
        if (frame_count <= 10)
        {
            if (otl == NULL)
            {
                fprintf(stderr, "Cannot open '%s'", fname_otladka);
                getchar();
                exit(-1);
            }
            int st = fprintf(otl, "frame_count = %d\n", frame_count);
            printf("st %x\n", st);
            st = fprintf(otl, "bs_size = %d\n", bs_size);
            printf("st %x\n", st);
            // Print matrix data to file:
            {
                static FILE*    matr_data  = fopen(fname_matr, "w");
                unsigned long*  p          = (unsigned long *)pQmatrixData;
                int             len        = sizeof(*pQmatrixData);
                int             num_el     = len / sizeof(unsigned long);

                if (!matr_data)
                {
                    fprintf(stderr, "Cannot open '%s'", fname_matr);
                    getchar();
                    exit(-1);
                }
                if (len % sizeof(unsigned long))
                {
                    num_el++;
                }
                for (int n = 0; n < num_el; n++)
                {
                    if (n == num_el-1)
                    {
                        fprintf(matr_data, "0x%x", p[n]);
                    }
                    else
                    {
                        fprintf(matr_data, "0x%x, ", p[n]);
                        if (0 == (n+1)%12)
                            fprintf(matr_data,"\n");
                    }
                }
                //fclose(matr_data);
            }
            // Print picture data to file:
            {
                static FILE*    pic_data = fopen(fname_pic, "w");
                unsigned long*  p        = (unsigned long *)vpPictureParam;
                int             len      = sizeof(*vpPictureParam);
                int             num_el   = len/sizeof(unsigned long);

                if (!pic_data)
                {
                    fprintf(stderr, "Cannot open '%s'", fname_pic);
                    exit(-1);
                }

                if (len % sizeof(unsigned long))
                {
                    num_el ++;
                }

                for (int n = 0; n < num_el; n++)
                {
                    if (n == num_el-1)
                    {
                        fprintf(pic_data, "0x%x", p[n]);
                    }
                    else
                    {
                        fprintf(pic_data, "0x%x, ", p[n]);
                        if (0 == (n + 1) % 4)
                        {
                            fprintf(pic_data,"\n");
                        }
                    }
                }
                //fclose(pic_data);
            }

            //  Print slice data to file:
            {
                int             n           = pSliceInfo - pSliceInfoBuffer;
                unsigned long*  p           = (unsigned long *)pSliceInfoBuffer;
                int             len         = sizeof(*pSliceInfoBuffer) * n;
                int             num_el      = len / sizeof(unsigned long);
                static FILE*    slice_data  = fopen(fname_slice, "w");

                if (!slice_data)
                {
                    fprintf(stderr, "Cannot open '%s'", fname_slice);
                    exit(-1);
                }

                if (len % sizeof(unsigned long))
                {
                    num_el++;
                }
                for (int n = 0; n < num_el; n++)
                {
                    if (n == num_el-1)
                    {
                        fprintf(slice_data, "0x%x", p[n]);
                    }
                    else
                    {
                        fprintf(slice_data, "0x%x, ", p[n]);
                        if (0 == (n + 1) % 12)
                        {
                            fprintf(slice_data, "\n");
                        }
                    }
                }
                //fclose(slice_data);
            }

            VAPictureParameterBufferMPEG2*  pPictureParam  = (VAPictureParameterBufferMPEG2*)vpPictureParam;
            if (slice_count == 0)
            {
                fprintf(otl, "Picture parameters:\n");
                fprintf(otl, "  horizontal_size = %d\n", pPictureParam->horizontal_size);
                fprintf(otl, "  vertical_size = %d\n", pPictureParam->vertical_size);
                fprintf(otl, "  forward_reference_picture = %d\n", pPictureParam->forward_reference_picture);
                fprintf(otl, "  backward_reference_picture = %d\n", pPictureParam->backward_reference_picture);
                fprintf(otl, "  picture_coding_type = %d\n", pPictureParam->picture_coding_type);
                fprintf(otl, "  f_code = %d\n", pPictureParam->f_code);
                fprintf(otl, "  picture_coding_extension.value = %x\n", pPictureParam->picture_coding_extension.value);
                fprintf(otl, "\n");
            }

            int                           n = pSliceInfo - pSliceInfoBuffer;
            VASliceParameterBufferMPEG2*  p = pSliceInfoBuffer;

            fprintf(otl, "slice_count %d; pSliceInfo %p; pSliceInfoBuffer %p;\n", n, pSliceInfo, pSliceInfoBuffer);
            for (int i = 0; i < n; i++)
            {
                fprintf(otl, "Slice %d\n", slice_count);
                fprintf(otl, "  slice_data_size = %x\n", p->slice_data_size);
                fprintf(otl, "  slice_data_offset = %x\n", p->slice_data_offset);
                fprintf(otl, "  slice_data_flag = %d\n", p->slice_data_flag);
                fprintf(otl, "  macroblock_offset = %d\n", p->macroblock_offset);
                fprintf(otl, "  slice_horizontal_position = %d\n", p->slice_horizontal_position);
                fprintf(otl, "  slice_vertical_position = %d\n", p->slice_vertical_position);
                fprintf(otl, "  quantiser_scale_code = %d\n", p->quantiser_scale_code);
                fprintf(otl, "  intra_slice_flag = %d\n", p->intra_slice_flag);
                fprintf(otl, "\n");
                slice_count++;
                p++;
            }
        }
        frame_count++;
    }
#endif  //OTLAD

    return UMC_OK;
}

Status
PackVA::SetBufferSize(
    Ipp32s          numMB,
    MPEG2FrameType  picture_coding_type,
    int             size_bs,    //ao: is local bs_size more precize?
    int             size_sl)
{
#if !defined(MFX_NO_EXCEPTIONS)
        try
#endif // #if !defined(MFX_NO_EXCEPTIONS)
        {
            UMCVACompBuffer*  CompBuf  = NULL;

            m_va->GetCompBuffer(
                VAPictureParameterBufferType,   //DXVA_PICTURE_DECODE_BUFFER
                &CompBuf,
                (Ipp32s)sizeof (VAPictureParameterBufferMPEG2));  //absent in win version
            CompBuf->SetDataSize((Ipp32s) (sizeof (VAPictureParameterBufferMPEG2))); //DXVA_PictureParameters
            CompBuf->FirstMb = 0;
            CompBuf->NumOfMB = 0;

            if (va_mode == VA_IT_W)
            {
                return UMC_ERR_UNSUPPORTED;
#ifdef MPEG2_IT_SUPPORTED
                m_va->GetCompBuffer(
                    DXVA_MACROBLOCK_CONTROL_BUFFER,
                    &CompBuf);
                Ipp32s size = (Ipp32s)((picture_coding_type == MPEG2_I_PICTURE) ?
                    numMB*sizeof(DXVA_MBctrl_I_OffHostIDCT_1) :
                    numMB*sizeof(DXVA_MBctrl_P_OffHostIDCT_1));
                CompBuf->SetDataSize(size);

                m_va->GetCompBuffer(
                    VAResidualDataBufferType,   //DXVA_RESIDUAL_DIFFERENCE_BUFFER,
                    &CompBuf);
                CompBuf->SetDataSize((Ipp32s) (totalNumCoef*sizeof(DXVA_TCoefSingle)));
#endif
            }
            else if (va_mode == VA_VLD_W)
            {
                m_va->GetCompBuffer(
                    VAIQMatrixBufferType,   //DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER
                    &CompBuf,
                    (Ipp32s)sizeof(VAIQMatrixBufferMPEG2));  //absent in win version
                CompBuf->SetDataSize((Ipp32s)sizeof(VAIQMatrixBufferMPEG2));    //ao: need?
                CompBuf->FirstMb = 0;
                CompBuf->NumOfMB = 0;

                m_va->GetCompBuffer(
                    VASliceParameterBufferType, //DXVA_SLICE_CONTROL_BUFFER,
                    &CompBuf,
                    (Ipp32s)sizeof(VASliceParameterBufferMPEG2)*size_sl);  //absent in win version
                CompBuf->SetDataSize((Ipp32s) ((Ipp8u*)pSliceInfo - (Ipp8u*)pSliceInfoBuffer));    //ao: need?
                CompBuf->FirstMb = 0;
                CompBuf->NumOfMB = numMB;

                m_va->GetCompBuffer(
                    VASliceDataBufferType,  //DXVA_BITSTREAM_DATA_BUFFER,
                    &CompBuf,
                    bs_size);   //ao: or size_bs?!
                CompBuf->SetDataSize(bs_size);    //ao: need?
                CompBuf->FirstMb = 0;
                CompBuf->NumOfMB = numMB;
            }
        }
#if !defined(MFX_NO_EXCEPTIONS)
        catch (...)
        {
            return UMC_ERR_NOT_ENOUGH_BUFFER;
        }
#endif // #if !defined(MFX_NO_EXCEPTIONS)

    return UMC_OK;
}

#endif //UMC_VA_LINUX

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
