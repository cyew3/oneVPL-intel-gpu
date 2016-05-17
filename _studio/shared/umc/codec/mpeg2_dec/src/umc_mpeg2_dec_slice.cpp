/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <vector>
#include "umc_mpeg2_dec_base.h"

#pragma warning(disable: 4244)

using namespace UMC;

#ifdef UMC_VA_DXVA
Ipp32u DistToNextSlice(mfxEncryptedData *encryptedData, PAVP_COUNTER_TYPE counterMode)
{
    if(encryptedData->Next)
    {
        Ipp64u counter1 = encryptedData->CipherCounter.Count;
        Ipp64u counter2 = encryptedData->Next->CipherCounter.Count;
        Ipp64u zero = 0xffffffffffffffff;
        if(counterMode == PAVP_COUNTER_TYPE_A)
        {
            counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
            counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
            zero = 0x00000000ffffffff;
        }
        else
        {
            counter1 = LITTLE_ENDIAN_SWAP64(counter1);
            counter2 = LITTLE_ENDIAN_SWAP64(counter2);
        }
        if(counter2 < counter1)
        {
            return (Ipp32u)(counter2 +(zero - counter1))*16;
        }
        else
        {
            return (Ipp32u)(counter2 - counter1)*16;
        }
    }
    else
    {
          int alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
          if(alignedSize & 0xf)
              alignedSize= alignedSize + (0x10 - (alignedSize & 0xf));
          return alignedSize;
    }
}
//slice size with alignment
Ipp32u SliceSize(mfxEncryptedData *first, PAVP_COUNTER_TYPE counterMode, 
                 Ipp32u &overlap)
{
    mfxEncryptedData *temp = first;
    int size = temp->DataLength + temp->DataOffset;
    if(size & 0xf)
      size= size + (0x10 - (size & 0xf));

    int retSize = size - overlap;
    if(temp->Next)
    {
        Ipp64u counter1 = temp->CipherCounter.Count;
        Ipp64u counter2 = temp->Next->CipherCounter.Count;
        Ipp64u zero = 0xffffffffffffffff;
        if(counterMode == PAVP_COUNTER_TYPE_A)
        {
            counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
            counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
            zero = 0x00000000ffffffff;
        }
        else
        {
            counter1 = LITTLE_ENDIAN_SWAP64(counter1);
            counter2 = LITTLE_ENDIAN_SWAP64(counter2);
        }
        Ipp64u endCounter = counter1 + size / 16;
        if(endCounter >= counter2)
        {
          overlap = (Ipp32u)(endCounter - counter2)*16;
        }
        else
        {
          overlap = (Ipp32u)(endCounter + (zero- counter2))*16;
        }
    }
    else
    {
        overlap=0;
    }

    return retSize;
}

//check correctness of encrypted data
Status CheckData(mfxEncryptedData *first, PAVP_COUNTER_TYPE counterMode)
{
    mfxEncryptedData * temp = first;
    for (; temp; )
    {
        if(temp->Next)
        {
            Ipp64u counter1 = temp->CipherCounter.Count;
            Ipp64u counter2 = temp->Next->CipherCounter.Count;
            Ipp64u zero = 0xffffffffffffffff;
            if(counterMode == PAVP_COUNTER_TYPE_A)
            {
                counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                zero = 0x00000000ffffffff;
            }
            else
            {
                counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                counter2 = LITTLE_ENDIAN_SWAP64(counter2);
            }
            //second slice offset starts after the first, there is no counter overall
            if(counter1 <= counter2)
            {
                Ipp32u size = temp->DataLength + temp->DataOffset;
                if(size & 0xf)
                    size= size+(0x10 - (size & 0xf));
                Ipp32u sizeCount = (Ipp32u)(counter2 - counter1) *16;
                //if we have hole between slices
                if(sizeCount > size)
                    return UMC_ERR_INVALID_STREAM;
                else //or overlap between slices is different in different buffers
                    if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                        return UMC_ERR_INVALID_STREAM;
            }
            else //there is counter overall or second slice offset starts earlier then the first one
            {
                //length of the second slice offset in 16-bytes blocks
                Ipp64u offsetCounter = (temp->Next->DataOffset + 15)/16;
                // no counter2 overall
                if(zero - counter2 > offsetCounter)
                {
                    Ipp32u size = temp->DataLength + temp->DataOffset;
                    if(size & 0xf)
                        size= size+(0x10 - (size & 0xf));
                    Ipp64u counter3 = size / 16;
                    // no zero during the first slice
                    if(zero - counter1 >= counter3)
                    {
                        //second slice offset contains the first slice with offset
                        if(counter2 + offsetCounter >= counter3)
                        {
                            temp->Next->Data += (Ipp32u)(counter1 - counter2) * 16;
                            temp->Next->DataOffset -= (Ipp32u)(counter1 - counter2) * 16;
                            temp->Next->CipherCounter = temp->CipherCounter;
                            //overlap between slices is different in different buffers
                            if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                                return UMC_ERR_INVALID_STREAM;
                        }
                        else //overlap between slices data or hole between slices
                        {
                            return UMC_ERR_INVALID_STREAM;
                        }
                    }
                    else
                    {
                        //size of data between counters
                        Ipp32u sizeCount = (Ipp32u)(counter2 + (zero - counter1)) *16;
                        //hole between slices
                        if(sizeCount > size)
                            return UMC_ERR_INVALID_STREAM;
                        else //or overlap between slices is different in different buffers
                            if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                                return UMC_ERR_INVALID_STREAM;
                    }
                }
                else
                {
                    temp->Next->Data += (Ipp32u)(counter1 - counter2) * 16;
                    temp->Next->DataOffset -= (Ipp32u)(counter1 - counter2) * 16;
                    temp->Next->CipherCounter = temp->CipherCounter;
                    //overlap between slices is different in different buffers
                    if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                        return UMC_ERR_INVALID_STREAM;
                }
            }
        }
        temp = temp->Next;
    }

    return UMC_OK;
}

#endif //#ifdef UMC_VA_DXVA

Status MPEG2VideoDecoderBase::DecodeSliceHeader(IppVideoContext *video, int task_num)
{
    Ipp32u extra_bit_slice;
    Ipp32u code;
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    Ipp32s bytes_remain = 0;
    Ipp32s bit_pos = 0;
#if defined(UMC_VA_LINUX)
    Ipp32u start_code;
#endif
#endif
    bool isCorrupted = false;

    if (!video)
    {
        return UMC_ERR_NULL_PTR;
    }

    FIND_START_CODE(video->bs, code)
#if defined(UMC_VA_LINUX)
    start_code = code;
#endif

    if(code == (Ipp32u)UMC_ERR_NOT_ENOUGH_DATA)
    {
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
        if (VA_VLD_W == pack_w.va_mode && false == pack_w.IsProtectedBS)
        {
            // update latest slice information
            Ipp8u *start_ptr = GET_BYTE_PTR(video->bs);
            Ipp8u *end_ptr = GET_END_PTR(video->bs);
            Ipp8u *ptr = start_ptr;
            Ipp32u count = 0;

            // calculate number of bytes of slice data
            while (ptr < end_ptr && (ptr[0] || ptr[1] || ptr[2] || ptr[3]))
            {
                ptr++;
                count++;
            }

#if defined(UMC_VA_DXVA)
            // update slice info structures (+ extra 10 bytes, this is @to do@)
        if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
            pack_w.pSliceInfo[-1].dwSliceBitsInBuffer = 8 * (count + 10);
#elif defined(UMC_VA_LINUX)
            // update slice info structures (+ extra 10 bytes, this is @to do@)
            if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                pack_w.pSliceInfo[-1].slice_data_size = count + 10;
            //fprintf(otl, "slice_data_size %x in DecodeSliceHeader(%d)\n", pack_w.pSliceInfo[-1].slice_data_size, __LINE__);
#endif
        }
#endif

        SKIP_TO_END(video->bs);

        return UMC_ERR_NOT_ENOUGH_DATA;
    }

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    if(pack_w.va_mode == VA_VLD_W || pack_w.is_analyzer) {
        // start of slice code
        bytes_remain = (Ipp32s)GET_REMAINED_BYTES(video->bs);
        bit_pos = (Ipp32s)GET_BIT_OFFSET(video->bs);
#ifdef UMC_VA_DXVA
        if(pack_w.IsProtectedBS && pack_w.pSliceInfo == pack_w.pSliceInfoBuffer)
        {
            Status sts=CheckData(pack_w.curr_encryptedData,
                (PAVP_COUNTER_TYPE)pack_w.m_va->GetProtectedVA()->GetCounterMode());
                if(sts < 0)
                    return sts;
        }
        if(pack_w.pSliceInfo > pack_w.pSliceInfoBuffer)
        {
             Ipp32s dsize = 0;
             Ipp32u overlap = pack_w.overlap;
             pack_w.pSliceInfo[-1].dwSliceBitsInBuffer -= bytes_remain*8;
             if(!pack_w.IsProtectedBS)
             {
                dsize = pack_w.pSliceInfo[-1].dwSliceBitsInBuffer/8;
                pack_w.bs_size = pack_w.pSliceInfo[-1].dwSliceDataLocation + dsize;
             }
             else
             {
                dsize = SliceSize(pack_w.curr_encryptedData,
                     (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                     overlap);
                overlap = overlap - pack_w.overlap;
                pack_w.overlap += overlap;
                 pack_w.bs_size += dsize;
             }

            if(pack_w.bs_size >= pack_w.bs_size_getting ||
                (Ipp32s)((Ipp8u*)pack_w.pSliceInfo - (Ipp8u*)pack_w.pSliceInfoBuffer) >= (Ipp32s)(pack_w.slice_size_getting-sizeof(DXVA_SliceInfo)))
            {
                bool slice_split = false;
                DXVA_SliceInfo s_info;
                Ipp32s sz = 0, sz_align = 0;

                memcpy_s(&s_info,sizeof(DXVA_SliceInfo),&pack_w.pSliceInfo[-1],sizeof(DXVA_SliceInfo));

                pack_w.bs_size -= dsize;
                pack_w.overlap -= overlap;

                pack_w.pSliceInfo--;

                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {
                    if(pack_w.IsProtectedBS)
                    {
                        Ipp32s NumSlices = pack_w.pSliceInfo - pack_w.pSliceInfoBuffer;
                        mfxEncryptedData *encryptedData = pack_w.curr_bs_encryptedData;
                        pack_w.bs_size = pack_w.add_to_slice_start;//0;
                        pack_w.overlap = 0;

                        for (int i = 0; i < NumSlices; i++)
                        {
                            if (!encryptedData)
                                return UMC_ERR_DEVICE_FAILED;
                        pack_w.bs_size += SliceSize(encryptedData,
                            (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                            pack_w.overlap);

                            encryptedData = encryptedData->Next;
                        }

                        sz = sz_align = pack_w.bs_size;
                        pack_w.is_bs_aligned_16 = true;

                        if(pack_w.bs_size & 0xf)
                        {
                            sz_align = ((pack_w.bs_size >> 4) << 4) + 16;
                            pack_w.bs_size = sz_align;
                            pack_w.is_bs_aligned_16 = false;
                        }

                        if(!pack_w.is_bs_aligned_16)
                        {
                            std::vector<Ipp8u> buf(sz_align);
                            NumSlices++;
                            encryptedData = pack_w.curr_bs_encryptedData;
                            Ipp8u *ptr = &buf[0];
                        Ipp8u *pBuf=ptr;
                            for (int i = 0; i < NumSlices; i++)
                            {
                                if (!encryptedData)
                                    return UMC_ERR_DEVICE_FAILED;
                            Ipp32u alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                            if(alignedSize & 0xf)
                                alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));
                                if(i < NumSlices - 1)
                                {
                                ippsCopy_8u(encryptedData->Data ,pBuf, alignedSize);
                                Ipp32u diff = (Ipp32u)(ptr - pBuf);
                                ptr += alignedSize - diff;
                                Ipp64u counter1 = encryptedData->CipherCounter.Count;
                                Ipp64u counter2 = encryptedData->Next->CipherCounter.Count;
                                Ipp64u zero = 0xffffffffffffffff;
                                if((pack_w.m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                                    counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                                    zero = 0x00000000ffffffff;
                                }
                                else
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                                    counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                                }
                                if(counter2 < counter1)
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 +(zero - counter1))*16;
                                }
                                else
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 - counter1)*16;
                                }
                            }
                            else
                            {
                                ippsCopy_8u(encryptedData->Data,pBuf, sz_align - sz);
                            }

                                encryptedData = encryptedData->Next;
                            }
                            ptr = &buf[0] + sz_align - 16;
                            ippsCopy_8u(pack_w.add_bytes,ptr, (sz - (sz_align - 16)));
                        }
                    }


mm:                 Ipp32s numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE) ?
                                        sequenceHeader.numMB[task_num] : sequenceHeader.numMB[task_num]/2;

                    if(pack_w.SetBufferSize(numMB,PictureHeader[task_num].picture_coding_type)!= UMC_OK)
                        return UMC_ERR_NOT_ENOUGH_BUFFER;

                    Status umcRes = pack_w.SaveVLDParameters(&sequenceHeader,&PictureHeader[task_num],
                        pack_w.pSliceStart,
                        &frame_buffer,task_num,((m_ClipInfo.clip_info.height + 15) >> 4));

                    if (UMC_OK != umcRes)
                        return umcRes;

                    umcRes = pack_w.m_va->Execute();
                    if (UMC_OK != umcRes)
                        return umcRes;

                    pack_w.add_to_slice_start = 0;

                    if(pack_w.IsProtectedBS)
                    {
                        if(!pack_w.is_bs_aligned_16)
                        {
                            pack_w.add_to_slice_start = sz - (sz_align - 16);
                        }
                    }
                }//if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)

                pack_w.InitBuffers(0, 0);

                memcpy_s(&pack_w.pSliceInfo[0],sizeof(DXVA_SliceInfo),&s_info,sizeof(DXVA_SliceInfo));
                pack_w.pSliceStart += pack_w.pSliceInfo[0].dwSliceDataLocation;
                pack_w.pSliceInfo[0].dwSliceDataLocation = 0;
                if(!pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.pSliceInfo[0].dwSliceBitsInBuffer/8;
                }
                else
                {
                        pack_w.bs_size = pack_w.add_to_slice_start;
                        pack_w.bs_size += SliceSize(pack_w.curr_encryptedData,
                            (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                            pack_w.overlap);
                }

                if(pack_w.bs_size >= pack_w.bs_size_getting)
                {
                    slice_split = true;
                    pack_w.pSliceInfo->dwSliceBitsInBuffer =
                        (pack_w.bs_size_getting - 1)*8;
                    pack_w.pSliceInfo->wBadSliceChopping = 1;
                    pack_w.bs_size -= (pack_w.bs_size_getting - 1);
                    s_info.dwSliceDataLocation = (pack_w.bs_size_getting - 1);
                    s_info.dwSliceBitsInBuffer -= pack_w.pSliceInfo->dwSliceBitsInBuffer;
                    pack_w.pSliceInfo++;
                    goto mm;
                }
                else
                {
                    if(slice_split)
                        pack_w.pSliceInfo->wBadSliceChopping = 2;
                }

                pack_w.pSliceInfo++;
            }
            if(pack_w.IsProtectedBS)
                pack_w.curr_encryptedData = pack_w.curr_encryptedData->Next;
        }
        else
        {
            pack_w.pSliceStart = GET_START_PTR(Video[task_num][0]->bs)+bit_pos/8;
            pack_w.bs_size = 0;
            pack_w.overlap = 0;
        }
#elif defined UMC_VA_LINUX
        if(pack_l.va_mode == VA_VLD_L) {
            if(pack_l.pSliceInfo > pack_l.pSliceInfoBuffer)
            {
                pack_l.pSliceInfo[-1].slice_data_size -= bytes_remain;

                pack_l.bs_size = pack_l.pSliceInfo[-1].slice_data_offset
                               + pack_l.pSliceInfo[-1].slice_data_size;    //AO: added by me
            }
            else
            {
                pack_l.pSliceStart = GET_START_PTR(Video[task_num][0]->bs)+bit_pos/8;
                pack_l.bs_size = 0;
            }
        }
#endif
    }
#endif

    if(code == PICTURE_START_CODE ||
       code > 0x1AF)
    {
      return UMC_ERR_NOT_ENOUGH_DATA;
    }
    // if (video) redundant checking
    {
        SKIP_BITS_32(video->bs);
    }

    video->slice_vertical_position = (code & 0xff);

    if(video->clip_info_height > 2800)
    {
        if(video->slice_vertical_position > 0x80)
          return UMC_ERR_INVALID_STREAM;
        GET_TO9BITS(video->bs, 3, code)
        video->slice_vertical_position += code << 7;
    }
    if( video->slice_vertical_position > PictureHeader[task_num].max_slice_vert_pos)
    {
        video->slice_vertical_position = PictureHeader[task_num].max_slice_vert_pos;

#ifdef UMC_VA_DXVA
        if(pack_w.pSliceInfo > pack_w.pSliceInfoBuffer)
            pack_w.pSliceInfo[-1].dwSliceBitsInBuffer += bytes_remain*8;
#endif
#ifdef UMC_VA_LINUX
        if(pack_l.pSliceInfo > pack_l.pSliceInfoBuffer)
            pack_l.pSliceInfo[-1].slice_data_size += bytes_remain;
#endif

        isCorrupted = true;
        return UMC_WRN_INVALID_STREAM;
    }

    if((sequenceHeader.extension_start_code_ID[task_num] == SEQUENCE_SCALABLE_EXTENSION_ID) &&
        (sequenceHeader.scalable_mode[task_num] == DATA_PARTITIONING))
    {
        GET_TO9BITS(video->bs, 7, code)
        return UMC_ERR_UNSUPPORTED;
    }

    GET_TO9BITS(video->bs, 5, video->cur_q_scale)
    if(video->cur_q_scale == 0)
    {
        isCorrupted = true;
        //return UMC_ERR_INVALID_STREAM;
    }

    GET_1BIT(video->bs,extra_bit_slice)
    while(extra_bit_slice)
    {
        GET_TO9BITS(video->bs, 9, code)
        extra_bit_slice = code & 1;
    }

#if defined UMC_VA_DXVA
    if(pack_w.va_mode == VA_VLD_W || pack_w.is_analyzer) {
      if (GET_REMAINED_BYTES(video->bs) <= 0) 
      {
          isCorrupted = true;
          return UMC_ERR_INVALID_STREAM;
      }
      pack_w.pSliceInfo->wMBbitOffset = (WORD)(GET_BIT_OFFSET(video->bs) - bit_pos);
      pack_w.pSliceInfo->wVerticalPosition = (WORD)(video->slice_vertical_position - 1);

      pack_w.pSliceInfo->dwSliceDataLocation = (DWORD)(GET_START_PTR(Video[task_num][0]->bs)+bit_pos/8-pack_w.pSliceStart);

      pack_w.pSliceInfo->bStartCodeBitOffset = 0;
      pack_w.pSliceInfo->wQuantizerScaleCode = (WORD)video->cur_q_scale;
      pack_w.pSliceInfo->wBadSliceChopping = 0;
#ifdef ELK
      pack_w.pSliceInfo->wHorizontalPosition = 0;
#else
      pack_w.pSliceInfo->wHorizontalPosition = 1;
#endif
      pack_w.pSliceInfo->dwSliceBitsInBuffer = bytes_remain*8;
      // assume slices are ordered
#ifdef ELK
      pack_w.pSliceInfo->wNumberMBsInSlice = (WORD)sequenceHeader.mb_width[task_num];
#else
      pack_w.pSliceInfo->wNumberMBsInSlice = 0;//sequenceHeader.mb_width;
#endif

      pack_w.pSliceInfo->bReservedBits = 0;
      pack_w.pSliceInfo->wBadSliceChopping = 0;
    }
#elif defined UMC_VA_LINUX

    if(pack_l.va_mode == VA_VLD_L) {
    
      if (GET_REMAINED_BYTES(video->bs) <= 0) {
        return UMC_ERR_INVALID_STREAM;
      }
      pack_l.pSliceInfo->macroblock_offset = GET_BIT_OFFSET(video->bs) - bit_pos;
      pack_l.pSliceInfo->slice_data_offset = GET_START_PTR(Video[task_num][0]->bs)+bit_pos/8-pack_l.pSliceStart;
      pack_l.pSliceInfo->quantiser_scale_code = video->cur_q_scale;
      pack_l.pSliceInfo->slice_data_size = bytes_remain;
      pack_l.pSliceInfo->intra_slice_flag = PictureHeader[task_num].picture_coding_type == MPEG2_I_PICTURE;

      if (video->stream_type != MPEG1_VIDEO)
      {
           const int field_pic = PictureHeader[task_num].picture_structure != FRAME_PICTURE;
           pack_l.pSliceInfo->slice_vertical_position = (start_code - 0x00000101) << field_pic; //SLICE_MIN_START_CODE 0x00000101
           if(BOTTOM_FIELD == PictureHeader[task_num].picture_structure)
               ++pack_l.pSliceInfo->slice_vertical_position;

           Ipp32u macroblock_address_increment=0;
           DECODE_MB_INCREMENT(video->bs, macroblock_address_increment);
           pack_w.pSliceInfo->slice_horizontal_position = macroblock_address_increment;
      }
      else
          pack_l.pSliceInfo->slice_vertical_position = video->slice_vertical_position-1;

      pack_l.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
    }
#endif


    if (video->stream_type != MPEG1_VIDEO)
    {
      video->cur_q_scale = q_scale[PictureHeader[task_num].q_scale_type][video->cur_q_scale];
    }

    RESET_PMV(video->PMV)

    video->macroblock_motion_forward  = 0;
    video->macroblock_motion_backward = 0;

    video->m_bNewSlice = true;

    if (true != frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted && true == isCorrupted)
    {
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted = true;
    }

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::DecodeSlice(IppVideoContext  *video, int task_num)
{

#if defined UMC_VA_DXVA
    if (pack_w. m_va && pack_w.va_mode != VA_NO) 
    {
        if (video->stream_type == MPEG1_VIDEO) 
        {
            return UMC_ERR_UNSUPPORTED;
        }

        if (video->color_format != YUV420) 
        {
            return UMC_ERR_UNSUPPORTED;
        }
    }
#elif defined UMC_VA_LINUX
    if (pack_l.va_mode != VA_NO) {
        return UMC_OK;
    }
#endif

    if (MPEG1_VIDEO == video->stream_type) 
    {
        return DecodeSlice_MPEG1(video, task_num);
    }

    try 
    {
        switch (video->color_format)
        {
            case YUV420:

                if (MPEG2_I_PICTURE == PictureHeader[task_num].picture_coding_type)
                {
                    return DecodeSlice_FrameI_420(video, task_num);
                }
                else 
                {
                    if (FRAME_PICTURE == PictureHeader[task_num].picture_structure)
                    {
                        return DecodeSlice_FramePB_420(video, task_num);
                    } 
                    else 
                    {
                        return DecodeSlice_FieldPB_420(video, task_num);
                    }
                }

                break;

            case YUV422:
                
                if (MPEG2_I_PICTURE == PictureHeader[task_num].picture_coding_type)
                {
                    return DecodeSlice_FrameI_422(video, task_num);
                }
                else 
                {
                    if (FRAME_PICTURE == PictureHeader[task_num].picture_structure) 
                    {
                        return DecodeSlice_FramePB_422(video, task_num);
                    }
                    else
                    {
                        return DecodeSlice_FieldPB_422(video, task_num);
                    }
                }
                
                break;
            
            default:
                return UMC_ERR_INVALID_STREAM;
        }
    }
    catch (...)
    {
        return UMC_ERR_INVALID_STREAM;
    }
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
