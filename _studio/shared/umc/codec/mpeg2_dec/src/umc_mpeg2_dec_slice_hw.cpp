// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)

#include <vector>
#include "umc_mpeg2_dec_hw.h"
#include "umc_mpeg2_dec_defs.h"
#include "umc_mpeg2_dec_bstream.h"
#include "mfx_trace.h"

// turn off the "conversion from 'type1' to 'type2', possible loss of data" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4244)
#endif

using namespace UMC;
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

//extern uint8_t default_intra_quantizer_matrix[64];
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

struct VLCEntry
{
    int8_t value;
    int8_t length;
};

static const VLCEntry MBAddrIncrTabB1_1[16] = {
        { -2 + 1, 48 - 48 }, { -2 + 1, 67 - 67 },
        { 2 + 5, 3 + 2 }, { 2 + 4, 3 + 2 },
        { 0 + 5, 1 + 3 }, { 4 + 1, 2 + 2 },
        { 3 + 1, 2 + 2 }, { 0 + 4, 3 + 1 },
        { 2 + 1, 2 + 1 }, { 0 + 3, 0 + 3 },
        { 1 + 2, 2 + 1 }, { 1 + 2, 1 + 2 },
        { 0 + 2, 2 + 1 }, { 1 + 1, 2 + 1 },
        { 0 + 2, 2 + 1 }, { 0 + 2, 2 + 1 },
};

static const VLCEntry MBAddrIncrTabB1_2[104] = {
    { 18 + 15, 5 + 6 }, { 18 + 14, 0 + 11 }, { 12 + 19, 7 + 4 }, { 6 + 24, 7 + 4 },
    { 4 + 25, 4 + 7 }, { 1 + 27, 0 + 11 }, { 4 + 23, 2 + 9 }, { 15 + 11, 3 + 8 },
    { 22 + 3, 1 + 10 }, { 15 + 9, 2 + 9 }, { 4 + 19, 5 + 6 }, { 0 + 22, 7 + 4 },
    { 2 + 19, 3 + 7 }, { 2 + 19, 7 + 3 }, { 1 + 19, 0 + 10 }, { 15 + 5, 6 + 4 },
    { 11 + 8, 4 + 6 }, { 0 + 19, 7 + 3 }, { 9 + 9, 8 + 2 }, { 10 + 8, 4 + 6 },
    { 8 + 9, 7 + 3 }, { 0 + 17, 6 + 4 }, { 5 + 11, 4 + 6 }, { 12 + 4, 3 + 7 },
    { 8 + 7, 3 + 5 }, { 2 + 13, 1 + 7 }, { 13 + 2, 2 + 6 }, { 1 + 14, 4 + 4 },
    { 0 + 15, 6 + 2 }, { 5 + 10, 5 + 3 }, { 4 + 11, 0 + 8 }, { 6 + 9, 3 + 5 },
    { 5 + 9, 4 + 4 }, { 5 + 9, 4 + 4 }, { 12 + 2, 3 + 5 }, { 5 + 9, 2 + 6 },
    { 9 + 5, 2 + 6 }, { 1 + 13, 7 + 1 }, { 1 + 13, 3 + 5 }, { 5 + 9, 4 + 4 },
    { 0 + 13, 1 + 7 }, { 12 + 1, 5 + 3 }, { 1 + 12, 7 + 1 }, { 2 + 11, 4 + 4 },
    { 1 + 12, 6 + 2 }, { 3 + 10, 3 + 5 }, { 6 + 7, 2 + 6 }, { 9 + 4, 6 + 2 },
    { 7 + 5, 2 + 6 }, { 11 + 1, 0 + 8 }, { 6 + 6, 5 + 3 }, { 3 + 9, 0 + 8 },
    { 4 + 8, 2 + 6 }, { 4 + 8, 7 + 1 }, { 10 + 2, 1 + 7 }, { 4 + 8, 6 + 2 },
    { 4 + 7, 2 + 6 }, { 4 + 7, 7 + 1 }, { 9 + 2, 6 + 2 }, { 8 + 3, 0 + 8 },
    { 7 + 4, 6 + 2 }, { 5 + 6, 3 + 5 }, { 9 + 2, 3 + 5 }, { 8 + 3, 0 + 8 },
    { 3 + 7, 5 + 3 }, { 7 + 3, 4 + 4 }, { 9 + 1, 4 + 4 }, { 8 + 2, 3 + 5 },
    { 3 + 7, 5 + 3 }, { 9 + 1, 6 + 2 }, { 7 + 3, 0 + 8 }, { 7 + 3, 2 + 6 },
    { 6 + 3, 6 + 1 }, { 3 + 6, 1 + 6 }, { 7 + 2, 6 + 1 }, { 8 + 1, 3 + 4 },
    { 4 + 5, 5 + 2 }, { 7 + 2, 1 + 6 }, { 2 + 7, 3 + 4 }, { 7 + 2, 1 + 6 },
    { 7 + 2, 0 + 7 }, { 7 + 2, 5 + 2 }, { 8 + 1, 0 + 7 }, { 4 + 5, 3 + 4 },
    { 0 + 9, 1 + 6 }, { 0 + 9, 4 + 3 }, { 4 + 5, 2 + 5 }, { 8 + 1, 5 + 2 },
    { 5 + 3, 0 + 7 }, { 6 + 2, 1 + 6 }, { 6 + 2, 6 + 1 }, { 4 + 4, 1 + 6 },
    { 0 + 8, 6 + 1 }, { 2 + 6, 1 + 6 }, { 2 + 6, 3 + 4 }, { 3 + 5, 6 + 1 },
    { 2 + 6, 2 + 5 }, { 3 + 5, 6 + 1 }, { 2 + 6, 6 + 1 }, { 6 + 2, 4 + 3 },
    { 3 + 5, 0 + 7 }, { 0 + 8, 0 + 7 }, { 6 + 2, 3 + 4 }, { 3 + 5, 1 + 6 },
};

int32_t DecoderMBInc(VideoContext  *video)
{
    int32_t macroblock_address_increment = 0;
    int32_t cc;

    for (;;)
    {
        SHOW_BITS(video->bs, 11, cc)
        if (cc >= 24)
            break;

        if (cc != 15) {
            if (cc != 8) // if macroblock_escape
            {
                return 1;
            }

            macroblock_address_increment += 33;
        }

        GET_BITS(video->bs, 11, cc);
    }

    if (cc >= 1024) {
        GET_BITS(video->bs, 1, cc);
        return macroblock_address_increment + 1;
    }

    int32_t cc1;

    if (cc >= 128) {
        cc >>= 6;
        GET_BITS(video->bs, MBAddrIncrTabB1_1[cc].length, cc1);
        return macroblock_address_increment + MBAddrIncrTabB1_1[cc].value;
    }

    cc -= 24;
    GET_BITS(video->bs, MBAddrIncrTabB1_2[cc].length, cc1);
    return macroblock_address_increment + MBAddrIncrTabB1_2[cc].value;
}

#ifdef UMC_VA_DXVA
uint32_t DistToNextSlice(mfxEncryptedData *encryptedData, PAVP_COUNTER_TYPE counterMode)
{
    if(encryptedData->Next)
    {
        unsigned long long counter1 = encryptedData->CipherCounter.Count;
        unsigned long long counter2 = encryptedData->Next->CipherCounter.Count;
        unsigned long long zero = 0xffffffffffffffff;
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
            return (uint32_t)(counter2 +(zero - counter1))*16;
        }
        else
        {
            return (uint32_t)(counter2 - counter1)*16;
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
uint32_t SliceSize(mfxEncryptedData *first, PAVP_COUNTER_TYPE counterMode,
                 uint32_t &overlap)
{
    mfxEncryptedData *temp = first;
    int size = temp->DataLength + temp->DataOffset;
    if(size & 0xf)
      size= size + (0x10 - (size & 0xf));

    int retSize = size - overlap;
    if(temp->Next)
    {
        unsigned long long counter1 = temp->CipherCounter.Count;
        unsigned long long counter2 = temp->Next->CipherCounter.Count;
        unsigned long long zero = 0xffffffffffffffff;
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
        unsigned long long endCounter = counter1 + size / 16;
        if(endCounter >= counter2)
        {
          overlap = (uint32_t)(endCounter - counter2)*16;
        }
        else
        {
          overlap = (uint32_t)(endCounter + (zero- counter2))*16;
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
            unsigned long long counter1 = temp->CipherCounter.Count;
            unsigned long long counter2 = temp->Next->CipherCounter.Count;
            unsigned long long zero = 0xffffffffffffffff;
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
                uint32_t size = temp->DataLength + temp->DataOffset;
                if(size & 0xf)
                    size= size+(0x10 - (size & 0xf));
                uint32_t sizeCount = (uint32_t)(counter2 - counter1) *16;
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
                unsigned long long offsetCounter = (temp->Next->DataOffset + 15)/16;
                // no counter2 overall
                if(zero - counter2 > offsetCounter)
                {
                    uint32_t size = temp->DataLength + temp->DataOffset;
                    if(size & 0xf)
                        size= size+(0x10 - (size & 0xf));
                    unsigned long long counter3 = size / 16;
                    // no zero during the first slice
                    if(zero - counter1 >= counter3)
                    {
                        //second slice offset contains the first slice with offset
                        if(counter2 + offsetCounter >= counter3)
                        {
                            temp->Next->Data += (uint32_t)(counter1 - counter2) * 16;
                            temp->Next->DataOffset -= (uint32_t)(counter1 - counter2) * 16;
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
                        uint32_t sizeCount = (uint32_t)(counter2 + (zero - counter1)) *16;
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
                    temp->Next->Data += (uint32_t)(counter1 - counter2) * 16;
                    temp->Next->DataOffset -= (uint32_t)(counter1 - counter2) * 16;
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

Status MPEG2VideoDecoderHW::Init(BaseCodecParams *pInit)
{
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if(!init)
        return UMC_ERR_INIT;

    if(!pack_w.SetVideoAccelerator(init->pVideoAccelerator))
        return UMC_ERR_UNSUPPORTED;

    return MPEG2VideoDecoderBase::Init(init);
}

Status MPEG2VideoDecoderHW::DecodeSliceHeader(VideoContext *video, int task_num)
{
    uint32_t extra_bit_slice;
    uint32_t code;
    int32_t bytes_remain = 0;
    int32_t bit_pos = 0;
    bool isCorrupted = false;

    if (!video)
    {
        return UMC_ERR_NULL_PTR;
    }

    FIND_START_CODE(video->bs, code)
#if defined(UMC_VA_LINUX)
    uint32_t start_code = code;
#endif

    if(code == (uint32_t)UMC_ERR_NOT_ENOUGH_DATA)
    {
        if (VA_VLD_W == pack_w.va_mode && false == pack_w.IsProtectedBS)
        {
            // update latest slice information
            uint8_t *start_ptr = GET_BYTE_PTR(video->bs);
            uint8_t *end_ptr = GET_END_PTR(video->bs);
            uint8_t *ptr = start_ptr;
            uint32_t count = 0;
            uint32_t zeroes = 0;
            //calculating the size of slice data, if 4 consecutive 0s are found, it indicates the end of the slice no further check is needed
            while (ptr < end_ptr)
            {
                if (*ptr++) zeroes = 0;
                else ++zeroes;
                ++count;
                if (zeroes == 4) break;
             }
            //adjust the count by reducing the number of 0s found, either 4 consecutive 0s or 0-3 0s existing at the end of the data. Based on the standard, they should be stuffing bytes.
            count -= zeroes;
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

        SKIP_TO_END(video->bs);

        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    if(pack_w.va_mode == VA_VLD_W) {
        // start of slice code
        bytes_remain = (int32_t)GET_REMAINED_BYTES(video->bs);
        bit_pos = (int32_t)GET_BIT_OFFSET(video->bs);
#ifdef UMC_VA_DXVA

#ifndef MFX_PROTECTED_FEATURE_DISABLE
        if(pack_w.IsProtectedBS && pack_w.pSliceInfo == pack_w.pSliceInfoBuffer)
        {
            Status sts=CheckData(pack_w.curr_encryptedData,
                (PAVP_COUNTER_TYPE)pack_w.m_va->GetProtectedVA()->GetCounterMode());
                if(sts < 0)
                    return sts;
        }
#endif

        if(pack_w.pSliceInfo > pack_w.pSliceInfoBuffer)
        {
             int32_t dsize = 0;
             uint32_t overlap = pack_w.overlap;
             pack_w.pSliceInfo[-1].dwSliceBitsInBuffer -= bytes_remain*8;

             dsize = pack_w.pSliceInfo[-1].dwSliceBitsInBuffer/8;
             pack_w.bs_size = pack_w.pSliceInfo[-1].dwSliceDataLocation + dsize;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
             if(pack_w.IsProtectedBS)
             {
                dsize = SliceSize(pack_w.curr_encryptedData,
                     (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                     overlap);
                overlap = overlap - pack_w.overlap;
                pack_w.overlap += overlap;
                pack_w.bs_size += dsize;
             }
#endif

            if(pack_w.bs_size >= pack_w.bs_size_getting ||
                (int32_t)((uint8_t*)pack_w.pSliceInfo - (uint8_t*)pack_w.pSliceInfoBuffer) >= (int32_t)(pack_w.slice_size_getting-sizeof(DXVA_SliceInfo)))
            {
                bool slice_split = false;
                DXVA_SliceInfo s_info = pack_w.pSliceInfo[-1];
                int32_t sz = 0, sz_align = 0;

                pack_w.bs_size -= dsize;
                pack_w.overlap -= overlap;

                pack_w.pSliceInfo--;

                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {
#ifndef MFX_PROTECTED_FEATURE_DISABLE
                    if(pack_w.IsProtectedBS)
                    {
                        int32_t NumSlices = pack_w.pSliceInfo - pack_w.pSliceInfoBuffer;
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
                            std::vector<uint8_t> buf(sz_align);
                            NumSlices++;
                            encryptedData = pack_w.curr_bs_encryptedData;
                            uint8_t *ptr = &buf[0];
                        uint8_t *pBuf=ptr;
                            for (int i = 0; i < NumSlices; i++)
                            {
                                if (!encryptedData)
                                    return UMC_ERR_DEVICE_FAILED;
                            uint32_t alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                            if(alignedSize & 0xf)
                                alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));
                                if(i < NumSlices - 1)
                                {
                                MFX_INTERNAL_CPY(pBuf, encryptedData->Data, alignedSize);
                                uint32_t diff = (uint32_t)(ptr - pBuf);
                                ptr += alignedSize - diff;
                                unsigned long long counter1 = encryptedData->CipherCounter.Count;
                                unsigned long long counter2 = encryptedData->Next->CipherCounter.Count;
                                unsigned long long zero = 0xffffffffffffffff;
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
                                    (unsigned char*)pBuf += (uint32_t)(counter2 +(zero - counter1))*16;
                                }
                                else
                                {
                                    (unsigned char*)pBuf += (uint32_t)(counter2 - counter1)*16;
                                }
                            }
                            else
                            {
                                MFX_INTERNAL_CPY(pBuf, encryptedData->Data,sz_align - sz);
                            }

                                encryptedData = encryptedData->Next;
                            }
                            ptr = &buf[0] + sz_align - 16;
                            MFX_INTERNAL_CPY(ptr, pack_w.add_bytes,(sz - (sz_align - 16)));
                        }
                    }
#endif

mm:                 int32_t numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE) ?
                                        sequenceHeader.numMB[task_num] : sequenceHeader.numMB[task_num]/2;

                    if(pack_w.SetBufferSize(numMB)!= UMC_OK)
                        return UMC_ERR_NOT_ENOUGH_BUFFER;

                    Status umcRes = pack_w.SaveVLDParameters(&sequenceHeader,&PictureHeader[task_num],
                        pack_w.pSliceStart,
                        &frame_buffer,task_num,((m_ClipInfo.clip_info.height + 15) >> 4));

                    if (UMC_OK != umcRes)
                        return umcRes;

                    umcRes = pack_w.m_va->Execute();
                    if (UMC_OK != umcRes)
                        return umcRes;

                    m_executed = true;

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

                pack_w.pSliceInfo[0] = s_info;
                pack_w.pSliceStart += pack_w.pSliceInfo[0].dwSliceDataLocation;
                pack_w.pSliceInfo[0].dwSliceDataLocation = 0;

                pack_w.bs_size = pack_w.pSliceInfo[0].dwSliceBitsInBuffer/8;
#ifndef MFX_PROTECTED_FEATURE_DISABLE
                if (pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.add_to_slice_start;
                    pack_w.bs_size += SliceSize(pack_w.curr_encryptedData,
                        (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                        pack_w.overlap);
                }
#endif

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
#ifndef MFX_PROTECTED_FEATURE_DISABLE
            if(pack_w.IsProtectedBS)
                pack_w.curr_encryptedData = pack_w.curr_encryptedData->Next;
#endif
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
    if(pack_w.va_mode == VA_VLD_W) {
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
        pack_w.pSliceInfo->wHorizontalPosition = 0;
        pack_w.pSliceInfo->dwSliceBitsInBuffer = bytes_remain*8;
        // assume slices are ordered
        pack_w.pSliceInfo->wNumberMBsInSlice = (WORD)sequenceHeader.mb_width[task_num];

        pack_w.pSliceInfo->bReservedBits = 0;
        pack_w.pSliceInfo->wBadSliceChopping = 0;
    }
#elif defined UMC_VA_LINUX

    if(pack_l.va_mode == VA_VLD_L)
    {
        if (GET_REMAINED_BYTES(video->bs) <= 0)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        uint32_t slice_idx = pack_w.pSliceInfo - pack_w.pSliceInfoBuffer;
        uint32_t num_allocated_slices = pack_w.slice_size_getting/sizeof(VASliceParameterBufferMPEG2);

        if (slice_idx < num_allocated_slices)
        {
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

                uint32_t macroblock_address_increment=1;


                if (IS_NEXTBIT1(video->bs))
                {
                    SKIP_BITS(video->bs, 1)
                }
                else
                {
                    macroblock_address_increment = DecoderMBInc(video);
                }
                macroblock_address_increment--;
                pack_w.pSliceInfo->slice_horizontal_position = macroblock_address_increment;
            }
            else
                pack_l.pSliceInfo->slice_vertical_position = video->slice_vertical_position-1;

            pack_l.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
        }
        else
            return UMC_ERR_FAILED;
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

Status MPEG2VideoDecoderHW::DecodeSlice(VideoContext  *video, int task_num)
{
#if defined UMC_VA_DXVA
    int32_t macroblock_address_increment = 1;

    int32_t remained = GET_REMAINED_BYTES(video->bs);

    if (remained == 0)
    {
        return UMC_OK;
    }
    else if (remained < 0)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    if (IS_NEXTBIT1(video->bs))
    {
        SKIP_BITS(video->bs, 1)
    }
    else
    {
        macroblock_address_increment = DecoderMBInc(video);
    }

    macroblock_address_increment--;
    pack_w.pSliceInfo->wHorizontalPosition   = (WORD)macroblock_address_increment;
    pack_w.pSliceInfo->wNumberMBsInSlice     = (WORD)(sequenceHeader.mb_width[task_num] - pack_w.pSliceInfo->wHorizontalPosition);

    if (pack_w.pSliceInfo->wHorizontalPosition > 0 && pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
        pack_w.pSliceInfo[-1].wNumberMBsInSlice  = (WORD)(macroblock_address_increment - pack_w.pSliceInfo[-1].wHorizontalPosition);
#else
    (void)video;
    (void)task_num;
#endif

    pack_w.pSliceInfo++;
    return UMC_OK;
}

Status MPEG2VideoDecoderHW::SaveDecoderState()
{
    frameBuffer_backup_previous = frameBuffer_backup;
    frameBuffer_backup = frame_buffer;
    b_curr_number_backup = sequenceHeader.b_curr_number;
    first_i_occure_backup = sequenceHeader.first_i_occure;
    frame_count_backup = sequenceHeader.frame_count;
    return UMC_OK;
}

Status MPEG2VideoDecoderHW::RestoreDecoderState()
{
    frame_buffer = frameBuffer_backup;
    frameBuffer_backup = frameBuffer_backup_previous;
    sequenceHeader.b_curr_number = b_curr_number_backup;
    sequenceHeader.first_i_occure = first_i_occure_backup;
    sequenceHeader.frame_count = frame_count_backup;
    return UMC_OK;
}

Status MPEG2VideoDecoderHW::RestoreDecoderStateAndRemoveLastField()
{
    frame_buffer = frameBuffer_backup_previous;
    sequenceHeader.b_curr_number = b_curr_number_backup;
    sequenceHeader.first_i_occure = first_i_occure_backup;
    sequenceHeader.frame_count = frame_count_backup;
    return UMC_OK;
}

Status MPEG2VideoDecoderHW::PostProcessFrame(int display_index, int task_num)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2VideoDecoderBase::PostProcessFrame");
    Status umcRes = UMC_OK;

      if(pack_w.va_mode != VA_NO)
      {
          if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo && pack_w.va_mode == VA_VLD_W)
          {
             // printf("save data at the end of frame\n");
#   if defined(UMC_VA_DXVA)   // part 1

             pack_w.bs_size = pack_w.pSliceInfo[-1].dwSliceDataLocation + pack_w.pSliceInfo[-1].dwSliceBitsInBuffer/8;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
             if(pack_w.IsProtectedBS)
             {
                 pack_w.bs_size += SliceSize(
                     pack_w.curr_encryptedData,
                     (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                     pack_w.overlap);
                 pack_w.is_bs_aligned_16 = true;
             }
#endif

             // printf("pack_w.bs_size = %d\n", pack_w.bs_size);
             // printf("pack_w.bs_size_getting = %d\n", pack_w.bs_size_getting);
            if ((pack_w.bs_size >= pack_w.bs_size_getting) ||
                ((int32_t)((uint8_t*)pack_w.pSliceInfo - (uint8_t*)pack_w.pSliceInfoBuffer) >=
                    (pack_w.slice_size_getting-(int32_t)sizeof(DXVA_SliceInfo))))
              {
                DXVA_SliceInfo s_info = pack_w.pSliceInfo[-1];
                bool slice_split = false;
                int32_t sz = 0, sz_align = 0;

                pack_w.pSliceInfo--;

                int size_bs = 0;
                int size_sl = 0;
                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {

#ifndef MFX_PROTECTED_FEATURE_DISABLE
                    if(pack_w.IsProtectedBS)
                    {
                        int32_t NumSlices = (int32_t)(pack_w.pSliceInfo - pack_w.pSliceInfoBuffer);
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
                            std::vector<uint8_t> buf(sz_align);
                            NumSlices++;
                            encryptedData = pack_w.curr_bs_encryptedData;
                            uint8_t *ptr = &buf[0];
                            uint8_t *pBuf=ptr;
                            for (int i = 0; i < NumSlices; i++)
                            {
                                if (!encryptedData)
                                    return UMC_ERR_DEVICE_FAILED;
                                uint32_t alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                                if(alignedSize & 0xf)
                                    alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));
                                if(i < NumSlices - 1)
                                {
                                    MFX_INTERNAL_CPY(pBuf, encryptedData->Data ,alignedSize);
                                    uint32_t diff = (uint32_t)(ptr - pBuf);
                                    ptr += alignedSize - diff;

                                    (unsigned char*)pBuf += DistToNextSlice(encryptedData,
                                        (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode());
                                }
                                else
                                {
                                    MFX_INTERNAL_CPY(pBuf, encryptedData->Data,sz_align - sz);
                                }

                                encryptedData = encryptedData->Next;
                            }
                            ptr = &buf[0] + sz_align - 16;
                            MFX_INTERNAL_CPY(ptr, pack_w.add_bytes,(sz - (sz_align - 16)));
                        }
                    }
#endif
#   elif defined UMC_VA_LINUX  // (part1)

            pack_w.bs_size = pack_w.pSliceInfo[-1].slice_data_offset
                           + pack_w.pSliceInfo[-1].slice_data_size;
            if (pack_w.bs_size >= pack_w.bs_size_getting)
            {
                VASliceParameterBufferMPEG2 s_info = pack_w.pSliceInfo[-1];
                bool slice_split = false;
                int32_t sz = 0, sz_align = 0;
                pack_w.pSliceInfo--;
                int size_bs = GET_END_PTR(Video[task_num][0]->bs) - GET_START_PTR(Video[task_num][0]->bs)+4;   // ao: I copied from BeginVAFrame. Is it ok? or == bs_size
                int size_sl = m_ClipInfo.clip_info.width*m_ClipInfo.clip_info.height/256;      // ao: I copied from BeginVAFrame. Is it ok?
                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {
#   endif  //UMC_VA_LINUX (part 1)
                  //  printf("Save previous slices\n");
mm:
                    int32_t numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
                        ? sequenceHeader.numMB[task_num]
                        : sequenceHeader.numMB[task_num]/2;

                    if (pack_w.SetBufferSize(
                        numMB,
                        size_bs,
                        size_sl)!= UMC_OK)
                    {
                      return UMC_ERR_NOT_ENOUGH_BUFFER;
                    }

                    umcRes = pack_w.SaveVLDParameters(
                        &sequenceHeader,
                        &PictureHeader[task_num],
                        pack_w.pSliceStart,
                        &frame_buffer,task_num,
                        ((m_ClipInfo.clip_info.height + 15) >> 4));
                    if (UMC_OK != umcRes)
                        return umcRes;

                    umcRes = pack_w.m_va->Execute();
                    if (UMC_OK != umcRes)
                        return umcRes;

                    m_executed = true;

                    pack_w.add_to_slice_start = 0;

                    if(pack_w.IsProtectedBS)
                    {
                        if(!pack_w.is_bs_aligned_16)
                        {
                            pack_w.add_to_slice_start = sz - (sz_align - 16);
                        }
                    }
                }//if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)

#    if defined(UMC_VA_DXVA)   // part 2
                pack_w.InitBuffers(0, 0);

                pack_w.pSliceInfo[0] = s_info;

                pack_w.pSliceStart += pack_w.pSliceInfo[0].dwSliceDataLocation;
                pack_w.pSliceInfo[0].dwSliceDataLocation = 0;

                pack_w.bs_size = pack_w.pSliceInfo[0].dwSliceBitsInBuffer/8;
#ifndef MFX_PROTECTED_FEATURE_DISABLE
                if(pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.add_to_slice_start;
                    pack_w.bs_size += SliceSize(
                        pack_w.curr_encryptedData,
                        (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                        pack_w.overlap);
                    pack_w.curr_encryptedData = pack_w.curr_encryptedData->Next;
                }
#endif

                if(pack_w.bs_size >= pack_w.bs_size_getting)
                {
                    slice_split = true;
                    pack_w.pSliceInfo->dwSliceBitsInBuffer = (pack_w.bs_size_getting - 1) * 8;
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

#elif defined(UMC_VA_LINUX)    // part 2

                pack_w.InitBuffers(size_bs, size_sl);

                pack_w.pSliceInfo[0] = s_info;

                pack_w.pSliceStart += pack_w.pSliceInfo[0].slice_data_offset;
                pack_w.pSliceInfo[0].slice_data_offset = 0;

                if(!pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.pSliceInfo[0].slice_data_size;
                }
                else
                {
                    // AO: add PAVP technologies
                }

                if(pack_w.bs_size >= pack_w.bs_size_getting)
                {
                    slice_split = true;
                    pack_w.pSliceInfo->slice_data_size = pack_w.bs_size_getting - 1;
                    pack_w.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_BEGIN;
                    pack_w.bs_size -= (pack_w.bs_size_getting - 1);
                    s_info.slice_data_offset = pack_w.bs_size_getting - 1;
                    s_info.slice_data_size -= pack_w.pSliceInfo->slice_data_size;
                    pack_w.pSliceInfo++;

                    goto mm;
                }
                else
                {
                    if(slice_split)
                        pack_w.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_END;
                }
#   endif   // UMC_VA_LINUX (part 2)

                pack_w.pSliceInfo++;
            }
            //  printf("Save last slice\n");

            umcRes = pack_w.SaveVLDParameters(
                &sequenceHeader,
                &PictureHeader[task_num],
                pack_w.pSliceStart,
                &frame_buffer,
                task_num);
            if (UMC_OK != umcRes)
            {
                return umcRes;
            }

            int32_t numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
                ? sequenceHeader.numMB[task_num]
                : sequenceHeader.numMB[task_num]/2;

              if(pack_w.SetBufferSize(numMB)!= UMC_OK)
                  return UMC_ERR_NOT_ENOUGH_BUFFER;

              umcRes = pack_w.m_va->Execute();
              if (UMC_OK != umcRes)
                  return UMC_ERR_DEVICE_FAILED;

              m_executed = true;
          }
          umcRes = pack_w.m_va->EndFrame();
          if (UMC_OK != umcRes || !m_executed)
              return UMC_ERR_DEVICE_FAILED;

          m_executed = false;

          if(display_index < 0)
          {
              return UMC_ERR_NOT_ENOUGH_DATA;
          }

          umcRes = PostProcessUserData(display_index);
          //pack_w.m_va->DisplayFrame(frame_buffer.frame_p_c_n[frame_buffer.retrieve    ].va_index,out_data);

          return UMC_OK;
      }//if(pack_w.va_mode != VA_NO)

    if (PictureHeader[task_num].picture_structure != FRAME_PICTURE &&
          frame_buffer.field_buffer_index[task_num] == 0)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    if (display_index < 0)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    umcRes = PostProcessUserData(display_index);

   return umcRes;
}

void MPEG2VideoDecoderHW::quant_matrix_extension(int task_num)
{
    int32_t i;
    uint32_t code;
    VideoContext* video = Video[task_num][0];
    int32_t load_intra_quantizer_matrix, load_non_intra_quantizer_matrix, load_chroma_intra_quantizer_matrix, load_chroma_non_intra_quantizer_matrix;
    uint8_t q_matrix[4][64] = {};

    GET_TO9BITS(video->bs, 4 ,code)
    GET_1BIT(video->bs,load_intra_quantizer_matrix)
    if(load_intra_quantizer_matrix)
    {
        for(i= 0; i < 64; i++) {
            GET_BITS(video->bs, 8, code);
            q_matrix[0][i] = (uint8_t)code;
        }
    }

    GET_1BIT(video->bs,load_non_intra_quantizer_matrix)
    if(load_non_intra_quantizer_matrix)
    {
        for(i= 0; i < 64; i++) {
            GET_BITS(video->bs, 8, code);
            q_matrix[1][i] = (uint8_t)code;
        }
    }

    GET_1BIT(video->bs,load_chroma_intra_quantizer_matrix);
    if(load_chroma_intra_quantizer_matrix && m_ClipInfo.color_format != YUV420)
    {
        for(i= 0; i < 64; i++) {
            GET_TO9BITS(video->bs, 8, code);
            q_matrix[2][i] = (uint8_t)code;
        }
    }

    GET_1BIT(video->bs,load_chroma_non_intra_quantizer_matrix);
    if(load_chroma_non_intra_quantizer_matrix && m_ClipInfo.color_format != YUV420)
    {
        for(i= 0; i < 64; i++) {
            GET_TO9BITS(video->bs, 8, code);
            q_matrix[3][i] = (uint8_t)code;
        }
    }

#if defined UMC_VA_DXVA
    if(pack_w.va_mode == VA_VLD_W) {
      if(load_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[0] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] = q_matrix[0][i];
        }
      }
      if(load_non_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[1] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] = q_matrix[1][i];
        }
      }
      if(load_chroma_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[2] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[2][i] = q_matrix[2][i];
        }
      }
      if(load_chroma_non_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[3] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[3][i] = q_matrix[3][i];
        }
      }
    }
#elif defined UMC_VA_LINUX
    if(pack_l.va_mode == VA_VLD_L) {
      if(load_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_intra_quantiser_matrix              = load_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.intra_quantiser_matrix[i] = q_matrix[0][i];
      }
      if(load_non_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_non_intra_quantiser_matrix          = load_non_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.non_intra_quantiser_matrix[i] = q_matrix[1][i];
      }
      if(load_chroma_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_chroma_intra_quantiser_matrix       = load_chroma_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = q_matrix[2][i];
      }
      if(load_chroma_non_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_chroma_non_intra_quantiser_matrix   = load_chroma_non_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = q_matrix[3][i];
      }
    }//if(va_mode == VA_VLD_L)
#endif

} //void quant_matrix_extension()

Status MPEG2VideoDecoderHW::ProcessRestFrame(int task_num)
{
    if (pack_w.m_va && bNeedNewFrame)
    {
        Status umcRes = BeginVAFrame(task_num);
        if (UMC_OK != umcRes)
            return umcRes;
        bNeedNewFrame = false;
    }

    Video[task_num][0]->stream_type = m_ClipInfo.stream_type;
    Video[task_num][0]->color_format = m_ClipInfo.color_format;
    Video[task_num][0]->clip_info_width = m_ClipInfo.clip_info.width;
    Video[task_num][0]->clip_info_height = m_ClipInfo.clip_info.height;

    return MPEG2VideoDecoderBase::ProcessRestFrame(task_num);
}


#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
Status MPEG2VideoDecoderHW::BeginVAFrame(int task_num)
#else
Status MPEG2VideoDecoderHW::BeginVAFrame(int)
#endif
{
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    int curr_index;
    PackVA *pack = &pack_w;
#ifdef UMC_VA_DXVA
    int size_bs = 0;
    int size_sl = 0;
#elif defined UMC_VA_LINUX
    int size_bs = GET_END_PTR(Video[task_num][0]->bs) - GET_START_PTR(Video[task_num][0]->bs)+4;
    int size_sl = m_ClipInfo.clip_info.width*m_ClipInfo.clip_info.height/256;
#endif

    if (pack->va_mode == VA_NO)
    {
        return UMC_OK;
    }

    {
        curr_index = frame_buffer.curr_index[task_num];
    }

    pack->m_SliceNum = 0;
    frame_buffer.frame_p_c_n[curr_index].va_index = pack->va_index;

    pack->field_buffer_index = frame_buffer.field_buffer_index[task_num];

    return pack->InitBuffers(size_bs, size_sl); //was "size" only
#else
    return UMC_OK;
#endif
}

Status MPEG2VideoDecoderHW::UpdateFrameBuffer(int , uint8_t* iqm, uint8_t*niqm)
{
#if defined UMC_VA_DXVA
    if(pack_w.va_mode == VA_VLD_W) {
      pack_w.QmatrixData.bNewQmatrix[0] = pack_w.QmatrixData.bNewQmatrix[2] = 1;
      if(iqm) {
        for(int i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] =
          pack_w.QmatrixData.Qmatrix[2][i] = iqm[i];
        }
      } else {
        for(int i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] =
          pack_w.QmatrixData.Qmatrix[2][i] = default_intra_quantizer_matrix[i];
        }
      }
      pack_w.QmatrixData.bNewQmatrix[1] = pack_w.QmatrixData.bNewQmatrix[3] = 1;
      if(niqm) {
        for(int i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] =
          pack_w.QmatrixData.Qmatrix[3][i] = niqm[i];
        }
      } else {
        for(int i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] =
          pack_w.QmatrixData.Qmatrix[3][i] = 16;
        }
      }
    }

#elif defined(UMC_VA_LINUX)

    if(pack_l.va_mode == VA_VLD_L)
    {
      pack_l.QmatrixData.load_intra_quantiser_matrix              = 1;
      pack_l.QmatrixData.load_non_intra_quantiser_matrix          = 1;
      pack_l.QmatrixData.load_chroma_intra_quantiser_matrix       = 1;
      pack_l.QmatrixData.load_chroma_non_intra_quantiser_matrix   = 1;
      if(iqm)
      {
        for(int i=0; i<64; i++)
        {
          pack_l.QmatrixData.intra_quantiser_matrix[i] = iqm[i];
          pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = iqm[i];
        }
      }
      else
      {
        for(int i=0; i<64; i++)
        {
          pack_l.QmatrixData.intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
          pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
        }
      }
      if(niqm)
      {
        for(int i=0; i<64; i++)
        {
            pack_l.QmatrixData.non_intra_quantiser_matrix[i] = niqm[i];
            pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = niqm[i];
        }
      }
      else
      {
        for(int i=0; i<64; i++)
        {
          pack_l.QmatrixData.non_intra_quantiser_matrix[i] = 16;
          pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = 16;
        }
      }
    }
#endif // UMC_VA_LINUX

    return UMC_OK;
}

#endif // #if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

#endif // MFX_ENABLE_MPEG2_VIDEO_DECODE
