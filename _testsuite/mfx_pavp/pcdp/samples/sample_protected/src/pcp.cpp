//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#include "sample_defs.h"

#include "pcp_session.h"
#include "avc_spl.h"
#include "vc1_spl.h"
#include "mpeg2_spl.h"
#include <stdlib.h>

mfxStatus PCPVideoDecode_Init(pcpSession *sessionPCP, mfxU32 codecId,
                                 pcpEncryptor *encryptor, bool isFullEncrypt)
{
    mfxStatus sts = MFX_ERR_NONE;

    // check error(s)
    if (MFX_CODEC_AVC != codecId && MFX_CODEC_MPEG2 != codecId && MFX_CODEC_VC1 != codecId )
        return MFX_ERR_UNSUPPORTED;
    if (NULL == sessionPCP)
        return MFX_ERR_NULL_PTR;

    _pcpSession *ctx = *sessionPCP;

    ctx = (_pcpSession*)malloc(sizeof(_pcpSession));
    if (NULL == ctx)
        return MFX_ERR_MEMORY_ALLOC;

    ctx->m_frame = NULL;
    ctx->m_pNALSplitter = NULL;
    ctx->m_encryptor = *encryptor;
    ctx->m_encryptedSliceBuffer = NULL;
    ctx->m_encryptedSliceBufferSize = 0;
    ctx->m_plainBuffer = NULL;
    ctx->m_plainBufferSize = 0;

    if (MFX_CODEC_AVC == codecId)
        ctx->m_pNALSplitter = new ProtectedLibrary::AVC_Spl();
    else if (MFX_CODEC_MPEG2 == codecId)
        ctx->m_pNALSplitter = new ProtectedLibrary::MPEG2_Spl();
    else if (MFX_CODEC_VC1 == codecId)
        ctx->m_pNALSplitter = new ProtectedLibrary::VC1_Spl();

    ctx->m_isFullEncryption = isFullEncrypt;

    *sessionPCP = ctx;
    return sts;
}

mfxStatus PCPVideoDecode_SetEncryptor(pcpSession sessionPCP, const pcpEncryptor *encryptor)
{
    mfxStatus sts = MFX_ERR_NONE;
    _pcpSession *ctx = sessionPCP;
    ctx->m_encryptor = *encryptor;
    return sts;
}

mfxStatus PCPVideoDecode_Close(pcpSession sessionPCP)
{
    mfxStatus sts = MFX_ERR_NONE;
    _pcpSession *ctx = sessionPCP;

    // check error(s)
    if (NULL == ctx)
        return MFX_ERR_NULL_PTR;

    if (NULL != ctx->m_encryptedSliceBuffer)
    {
        free(ctx->m_encryptedSliceBuffer);
        ctx->m_encryptedSliceBuffer = NULL;
        ctx->m_encryptedSliceBufferSize = 0;
    }

    if (NULL != ctx->m_plainBuffer)
    {
        free(ctx->m_plainBuffer);
        ctx->m_plainBuffer = NULL;
        ctx->m_plainBufferSize = 0;
    }

    if (NULL != ctx->m_pNALSplitter)
    {
        delete ctx->m_pNALSplitter;
        ctx->m_pNALSplitter = NULL;
    }

    free(ctx);
    ctx = NULL;

    return sts;
}

mfxStatus _PCPVideoDecode_PrepareNextFrame(pcpSession sessionPCP,
    mfxBitstream *in,
    mfxBitstream **out,
    bool isSparse)
{
    mfxStatus sts = MFX_ERR_NONE;
    _pcpSession *ctx = sessionPCP;
    bool isEncrypt=false;

    // check error(s)
    if (NULL == ctx || NULL == out)
        return MFX_ERR_NULL_PTR;

    *out = NULL;

    // get frame if it is not ready yet
    if (NULL == ctx->m_frame)
    {
        sts = ctx->m_pNALSplitter->GetFrame(in, &ctx->m_frame);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    if (ctx->m_plainBufferSize < ctx->m_frame->DataLength)
    {
        if (NULL != ctx->m_plainBuffer)
        {
            free(ctx->m_plainBuffer);
            ctx->m_plainBuffer = NULL;
            ctx->m_plainBufferSize = 0;
        }
        ctx->m_plainBuffer = (mfxU8*)malloc(ctx->m_frame->DataLength);
        if (NULL == ctx->m_plainBuffer)
            return MFX_ERR_MEMORY_ALLOC;
        ctx->m_plainBufferSize = ctx->m_frame->DataLength;
    }

    if(NULL != ctx->m_encryptor.Encrypt)
    {
        if(ctx->m_isFullEncryption)
            isEncrypt=true;
        else
        {
            for(int i=0; i<ctx->m_frame->SliceNum; i++)
            {
                if((ctx->m_frame->Slice[i].SliceType != ProtectedLibrary::TYPE_P)&& // not P slices
                   (ctx->m_frame->Slice[i].SliceType != ProtectedLibrary::TYPE_B))  //not B slices
                {
                    isEncrypt=true;
                    break;
                }
            }
        }
    }

    if (isEncrypt)
    {
        mfxAES128CipherCounter cipherCounter;

        // calculate encryption buffer size and counter values needed
        size_t encryptedBufferSize = 0;

        size_t size = 0;
        for (mfxU32 i = 0; i < ctx->m_frame->SliceNum; i++)
        {
            ProtectedLibrary::SliceSplitterInfo *slice = &ctx->m_frame->Slice[i];
            if (isSparse)
            {
                if (slice->DataOffset <= encryptedBufferSize)
                {
                    size = 0;
                }
                else
                {
                    size = slice->DataOffset - encryptedBufferSize;
                }
                encryptedBufferSize += size;
            }

            encryptedBufferSize += slice->DataLength;
            if (i == ctx->m_frame->FirstFieldSliceNum - 1 ||
                i == ctx->m_frame->SliceNum - 1)
            {
                if (encryptedBufferSize & 0xf)
                    encryptedBufferSize = encryptedBufferSize + (0x10 - (encryptedBufferSize & 0xf));
            }
        }

        // allocate enough encrypted slice data
        size_t neededSize =
            ctx->m_frame->SliceNum * sizeof(mfxEncryptedData) +
            encryptedBufferSize;
        if (ctx->m_encryptedSliceBufferSize < neededSize)
        {
            if (NULL != ctx->m_encryptedSliceBuffer)
            {
                free(ctx->m_encryptedSliceBuffer);
                ctx->m_encryptedSliceBuffer = NULL;
                ctx->m_encryptedSliceBufferSize = 0;
            }
            ctx->m_encryptedSliceBuffer = (mfxU8*)malloc(neededSize);
            if (NULL == ctx->m_encryptedSliceBuffer)
                return MFX_ERR_MEMORY_ALLOC;
            ctx->m_encryptedSliceBufferSize = (mfxU32)neededSize;
        }
        if (NULL != ctx->m_encryptedSliceBuffer)
            memset(ctx->m_encryptedSliceBuffer, 0, ctx->m_frame->SliceNum * sizeof(mfxEncryptedData));
        else
            return MFX_ERR_NULL_PTR;


        mfxEncryptedData *cur = (mfxEncryptedData*)
            ctx->m_encryptedSliceBuffer;
        mfxU8 *encryptedSlices =
            (mfxU8*)cur + ctx->m_frame->SliceNum * sizeof(mfxEncryptedData);
        mfxU32 freeEncryptedBufferSize=ctx->m_encryptedSliceBufferSize -
            ctx->m_frame->SliceNum * sizeof(mfxEncryptedData);
        mfxEncryptedData *last = NULL;

        mfxU8 *encryptedSlicesPtr = encryptedSlices;
        mfxEncryptedData *first = cur;

        mfxU32 framePos = 0;
        // We will skip copying slice data into output bitstream but want to leave
        // placeholder. Filling whole buffer with 0xff to ensure no control sequences
        // appears later in placeholders. That allows MediaSDK to calculate correct
        // value for DXVA_Slice_H264_Long::SliceBytesInBuffer
        memset(ctx->m_plainBuffer, 0xff, ctx->m_frame->DataLength);

        if (isSparse)
        {
            size = ctx->m_frame->Slice[0].DataOffset;
            encryptedSlicesPtr += size;
        }
        for (mfxU32 i = 0; i < ctx->m_frame->SliceNum; i++)
        {
            ProtectedLibrary::SliceSplitterInfo *slice = &ctx->m_frame->Slice[i];

            // copy data and slice header up to the slice data
            memcpy_s(ctx->m_plainBuffer + framePos, ctx->m_plainBufferSize - framePos, ctx->m_frame->Data + framePos,
                slice->DataOffset + slice->HeaderLength - framePos);
            framePos = slice->DataOffset + slice->DataLength;

            sts = ctx->m_pNALSplitter->PostProcessing(ctx->m_frame,i);
            if (sts != MFX_ERR_NONE)
                return MFX_ERR_MORE_DATA;

           // add encrypted slices list element and will it with slice header and data
            cur->Next = cur + 1;
            cur->Data = encryptedSlices;
            cur->DataOffset = (mfxU32)(encryptedSlicesPtr - encryptedSlices);
            MSDK_MEMCPY_BUF(cur->Data, cur->DataOffset, freeEncryptedBufferSize + cur->DataOffset,
                ctx->m_frame->Data + slice->DataOffset, slice->DataLength);

            // encryption checkpoint - encrypt accumulated bytes and fill counter value
            if (i == ctx->m_frame->FirstFieldSliceNum - 1 ||
                i == ctx->m_frame->SliceNum - 1)
            {
                cur->DataLength = slice->DataLength;
                encryptedSlicesPtr += cur->DataLength;

                // ensure memory size to encrypt aligned 16
                size_t size = encryptedSlicesPtr - encryptedSlices;
                if (0 != (size & 0xf))
                {
                    mfxU32 paddingSize = mfxU32(0x10 - (size & 0xf));
                    encryptedSlicesPtr += paddingSize;
                }

                sts = ctx->m_encryptor.Encrypt(
                    ctx->m_encryptor.pthis,
                    encryptedSlices,
                    encryptedSlices,
                    mfxU32(encryptedSlicesPtr - encryptedSlices),
                    &cipherCounter);
                if (sts != MFX_ERR_NONE)
                {
                    return sts;
                }
                MSDK_MEMCPY_VAR(first->CipherCounter, &cipherCounter, sizeof(mfxAES128CipherCounter));
                mfxEncryptedData *tmp_cur=first->Next;
                mfxEncryptedData *tmp_prev=first;
                while(tmp_prev != cur)
                {
                    tmp_cur->CipherCounter.Count = tmp_prev->CipherCounter.Count;
                    tmp_cur->CipherCounter.IV = tmp_prev->CipherCounter.IV;
                    tmp_prev = tmp_cur;
                    tmp_cur = tmp_cur->Next;
                }

                first = cur + 1;
                encryptedSlices = encryptedSlicesPtr;
            }
            else
            {
                cur->DataLength = slice->DataLength;
                mfxI64 size = 0;
                if (isSparse)
                {
                    size = (mfxI64)ctx->m_frame->Slice[i+1].DataOffset -
                        ((encryptedSlicesPtr - encryptedSlices) + slice->DataLength);
                    if (size < 0)
                        size = 0;
                    size += slice->DataLength;
                }
                else
                {
                    size = cur->DataLength;
                }
                encryptedSlicesPtr += size;
            }

            {
            mfxU32 offset = (mfxU32)(encryptedSlicesPtr - cur->Data);
            cur->MaxLength = (offset + 15) & ~0xf;
            freeEncryptedBufferSize-=(offset - cur->DataOffset);
            }
            last = cur;
            cur++;
        }
        memcpy_s(ctx->m_plainBuffer + framePos, ctx->m_plainBufferSize - framePos, ctx->m_frame->Data + framePos,
            ctx->m_frame->DataLength - framePos);
        if (NULL != last)
            last->Next = NULL;

        memset(&ctx->m_outBS, 0, sizeof(mfxBitstream));
        ctx->m_outBS.Data = ctx->m_plainBuffer;
        ctx->m_outBS.DataOffset = 0;
        ctx->m_outBS.DataLength = ctx->m_frame->DataLength;
        ctx->m_outBS.MaxLength = ctx->m_frame->DataLength;
        ctx->m_outBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
        ctx->m_outBS.TimeStamp = ctx->m_frame->TimeStamp;
        if (0 != ctx->m_frame->SliceNum)
            ctx->m_outBS.EncryptedData = (mfxEncryptedData*)ctx->m_encryptedSliceBuffer;
    }
    else
    {
        memcpy_s(ctx->m_plainBuffer, ctx->m_plainBufferSize, ctx->m_frame->Data, ctx->m_frame->DataLength);

        memset(&ctx->m_outBS, 0, sizeof(mfxBitstream));
        ctx->m_outBS.Data = ctx->m_plainBuffer;
        ctx->m_outBS.DataOffset = 0;
        ctx->m_outBS.DataLength = ctx->m_frame->DataLength;
        ctx->m_outBS.MaxLength = ctx->m_frame->DataLength;
        ctx->m_outBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
        ctx->m_outBS.TimeStamp = ctx->m_frame->TimeStamp;
    }
    ctx->m_pNALSplitter->ResetCurrentState();
    ctx->m_frame = NULL;

    *out = &ctx->m_outBS;

    return sts;
}

mfxStatus PCPVideoDecode_PrepareNextFrame(pcpSession sessionPCP,
    mfxBitstream *in,
    mfxBitstream **out,
    bool isSparse)
{
        return _PCPVideoDecode_PrepareNextFrame(sessionPCP, in, out, isSparse);
}