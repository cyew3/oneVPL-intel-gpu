//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{

    mfxStatus QueryHwCaps(mfxCoreInterface * pCore, ENCODE_CAPS_VP9 & caps, GUID guid)
    {
        std::auto_ptr<DriverEncoder> ddi;

        ddi.reset(CreatePlatformVp9Encoder(pCore));
        MFX_CHECK(ddi.get() != NULL, MFX_WRN_PARTIAL_ACCELERATION);

        mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(pCore, guid, 1920, 1088);
        MFX_CHECK_STS(sts);

        sts = ddi.get()->QueryEncodeCaps(caps);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    }

// uncompressed header packing

#define VP9_FRAME_MARKER 0x2

#define VP9_SYNC_CODE_0 0x49
#define VP9_SYNC_CODE_1 0x83
#define VP9_SYNC_CODE_2 0x42

#define FRAME_CONTEXTS_LOG2 2
#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)

#define QINDEX_BITS 8

#define MAX_TILE_WIDTH_B64 64
#define MIN_TILE_WIDTH_B64 4

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - MI_SIZE_LOG2)  // 64 = 2^6
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

    void WriteBit(BitBuffer &buf, mfxU8 bit)
    {
        const mfxU16 byteOffset = buf.bitOffset / CHAR_BIT;
        const mfxU8 bitsLeftInByte = CHAR_BIT - 1 - buf.bitOffset % CHAR_BIT;
        if (bitsLeftInByte == CHAR_BIT - 1)
        {
            buf.pBuffer[byteOffset] = mfxU8(bit << bitsLeftInByte);
        }
        else
        {
            buf.pBuffer[byteOffset] &= ~(1 << bitsLeftInByte);
            buf.pBuffer[byteOffset] |= bit << bitsLeftInByte;
        }
        buf.bitOffset = buf.bitOffset + 1;
    };

    void WriteLiteral(BitBuffer &buf, mfxU64 data, mfxU64 bits)
    {
        for (mfxI64 bit = bits - 1; bit >= 0; bit--)
        {
            WriteBit(buf, (data >> bit) & 1);
        }
    }

    void WriteColorConfig(BitBuffer &buf, VP9SeqLevelParam const &seqPar)
    {
        if (seqPar.profile >= PROFILE_2)
        {
            assert(seqPar.bitDepth > BITDEPTH_8);
            WriteBit(buf, seqPar.bitDepth == BITDEPTH_10 ? 0 : 1);
        }
        WriteLiteral(buf, seqPar.colorSpace, 3);
        if (seqPar.colorSpace != SRGB)
        {
            WriteBit(buf, seqPar.colorRange);
            if (seqPar.profile == PROFILE_1 || seqPar.profile == PROFILE_3)
            {
                assert(seqPar.subsamplingX != 1 || seqPar.subsamplingY != 1);
                WriteBit(buf, seqPar.subsamplingX);
                WriteBit(buf, seqPar.subsamplingY);
                WriteBit(buf, 0);  // unused
            }
            else
            {
                assert(seqPar.subsamplingX == 1 && seqPar.subsamplingY == 1);
            }
        }
        else
        {
            assert(seqPar.profile == PROFILE_1 || seqPar.profile == PROFILE_3);
            WriteBit(buf, 0);  // unused
        }
    }

    void WriteFrameSize(BitBuffer &buf, VP9FrameLevelParam const &framePar)
    {
        WriteLiteral(buf, framePar.width - 1, 16);
        WriteLiteral(buf, framePar.height - 1, 16);

        const mfxU8 renderFrameSizeDifferent = 0; // TODO: add support
        WriteBit(buf, renderFrameSizeDifferent);
        /*if (renderFrameSizeDifferent)
        {
            WriteLiteral(buf, framePar.renderWidth - 1, 16);
            WriteLiteral(buf, framePar.renderHeight - 1, 16);
        }*/
    }

    void WriteQIndexDelta(BitBuffer &buf, mfxI16 qDelta)
    {
        if (qDelta != 0)
        {
            WriteBit(buf, 1);
            WriteLiteral(buf, abs(qDelta), 4);
            WriteBit(buf, qDelta < 0);
        }
        else
        {
            WriteBit(buf, 0);
        }
    }

    mfxU16 WriteUncompressedHeader(BitBuffer &localBuf,
                                   Task const &task,
                                   VP9SeqLevelParam const &seqPar,
                                   BitOffsets &offsets)
    {
        VP9FrameLevelParam const &framePar = task.m_frameParam;

        Zero(offsets);

        WriteLiteral(localBuf, VP9_FRAME_MARKER, 2);

        // profile
        switch (seqPar.profile)
        {
            case PROFILE_0: WriteLiteral(localBuf, 0, 2); break;
            case PROFILE_1: WriteLiteral(localBuf, 2, 2); break;
            case PROFILE_2: WriteLiteral(localBuf, 1, 2); break;
            case PROFILE_3: WriteLiteral(localBuf, 6, 3); break;
            default: assert(0);
        }

        WriteBit(localBuf, 0);  // show_existing_frame
        WriteBit(localBuf, framePar.frameType);
        WriteBit(localBuf, framePar.showFarme);
        WriteBit(localBuf, framePar.errorResilentMode);

        if (framePar.frameType == KEY_FRAME) // Key frame
        {
            // sync code
            WriteLiteral(localBuf, VP9_SYNC_CODE_0, 8);
            WriteLiteral(localBuf, VP9_SYNC_CODE_1, 8);
            WriteLiteral(localBuf, VP9_SYNC_CODE_2, 8);

            // color config
            WriteColorConfig(localBuf, seqPar);

            // frame, render size
            WriteFrameSize(localBuf, framePar);
        }
        else // Inter frame
        {
            if (!framePar.showFarme)
            {
                WriteBit(localBuf, framePar.intraOnly);
            }

            if (!framePar.errorResilentMode)
            {
                WriteLiteral(localBuf, framePar.resetFrameContext, 2);
            }

            // prepare refresh frame mask
            mfxU8 refreshFamesMask = 0;
            for (mfxU8 i = 0; i < DPB_SIZE; i++)
            {
                refreshFamesMask |= (framePar.refreshRefFrames[i] << i);
            }

            if (framePar.intraOnly)
            {
                // sync code
                WriteLiteral(localBuf, VP9_SYNC_CODE_0, 8);
                WriteLiteral(localBuf, VP9_SYNC_CODE_1, 8);
                WriteLiteral(localBuf, VP9_SYNC_CODE_2, 8);

                // Note for profile 0, 420 8bpp is assumed.
                if (seqPar.profile > PROFILE_0)
                {
                    WriteColorConfig(localBuf, seqPar);
                }

                // refresh frame info
                WriteLiteral(localBuf, refreshFamesMask, REF_FRAMES);

                // frame, render size
                WriteFrameSize(localBuf, framePar);
            }
            else
            {
                WriteLiteral(localBuf, refreshFamesMask, REF_FRAMES);
                for (mfxI8 refFrame = LAST_FRAME; refFrame <= ALTREF_FRAME; refFrame ++)
                {
                    WriteLiteral(localBuf, framePar.refList[refFrame], REF_FRAMES_LOG2);
                    WriteBit(localBuf, framePar.refBiases[refFrame]);
                }

                // frame size with refs
                mfxU8 found = 0;

                for (mfxI8 refFrame = LAST_FRAME; refFrame <= ALTREF_FRAME; refFrame ++)
                {
                    // TODO: implement correct logic for [found] flag
                    WriteBit(localBuf, found);
                }

                if (!found)
                {
                    WriteLiteral(localBuf, framePar.width - 1, 16);
                    WriteLiteral(localBuf, framePar.height - 1, 16);
                }

                const mfxU8 renderFrameSizeDifferent = 0; // TODO: add support
                WriteBit(localBuf, renderFrameSizeDifferent);
                /*if (renderFrameSizeDifferent)
                {
                    WriteLiteral(localBuf, framePar.renderWidth - 1, 16);
                    WriteLiteral(localBuf, framePar.renderHeight - 1, 16);
                }*/

                WriteBit(localBuf, framePar.allowHighPrecisionMV);

                // interpolation filter syntax
                const mfxU8 filterToLiteralMap[] = { 1, 0, 2, 3 };

                WriteBit(localBuf, framePar.interpFilter == SWITCHABLE);
                if (framePar.interpFilter != SWITCHABLE)
                {
                    WriteLiteral(localBuf, filterToLiteralMap[framePar.interpFilter], 2);
                }
            }
        }

        if (!framePar.errorResilentMode)
        {
            WriteBit(localBuf, framePar.refreshFrameContext);
            WriteBit(localBuf, seqPar.frameParallelDecoding);
        }

        WriteLiteral(localBuf, framePar.frameContextIdx, FRAME_CONTEXTS_LOG2);

        offsets.BitOffsetForLFLevel = (mfxU16)localBuf.bitOffset;
        // loop filter syntax
        WriteLiteral(localBuf, framePar.lfLevel, 6);
        WriteLiteral(localBuf, framePar.sharpness, 3);

        WriteBit(localBuf, framePar.modeRefDeltaEnabled);

        if (framePar.modeRefDeltaEnabled)
        {
            WriteBit(localBuf, framePar.modeRefDeltaUpdate);
            if (framePar.modeRefDeltaUpdate)
            {
                offsets.BitOffsetForLFRefDelta = (mfxU16)localBuf.bitOffset;
                for (mfxI8 i = 0; i < MAX_REF_LF_DELTAS; i++)
                {
                    //const mfxI8 delta = framePar.lfRefDelta[i];
                    const mfxU8 delatChanged = 0; // TODO: add support for changing delta
                    WriteBit(localBuf, delatChanged);
                    /*if (delatChanged)
                    {
                        WriteLiteral(localBuf, abs(delta) & 0x3F, 6);
                        WriteBit(localBuf, delta < 0);
                    }*/
                }

                offsets.BitOffsetForLFModeDelta = (mfxU16)localBuf.bitOffset;
                for (mfxI8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
                {
                    //const mfxI8 delta = framePar.lfModeDelta[i];
                    const mfxU8 delatChanged = false; // TODO: add support for changing delta
                    WriteBit(localBuf, delatChanged);
                    /*if (delatChanged)
                    {
                        WriteLiteral(localBuf, abs(delta) & 0x3F, 6);
                        WriteBit(localBuf, delta < 0);
                    }*/
                }
            }
        }

        offsets.BitOffsetForQIndex = (mfxU16)localBuf.bitOffset;

        // quantization params
        WriteLiteral(localBuf, framePar.baseQIndex, QINDEX_BITS);
        WriteQIndexDelta(localBuf, framePar.qIndexDeltaLumaDC);
        WriteQIndexDelta(localBuf, framePar.qIndexDeltaChromaDC);
        WriteQIndexDelta(localBuf, framePar.qIndexDeltaChromaAC);

        offsets.BitOffsetForSegmentation = (mfxU16)localBuf.bitOffset;

        //segmentation
        WriteBit(localBuf, 0); // TODO: add support

        offsets.BitSizeForSegmentation = (mfxU16)localBuf.bitOffset - offsets.BitOffsetForSegmentation;

        // tile info
        mfxU8 minLog2TileCols = 0;
        mfxU8 maxLog2TileCols = 1;
        mfxU8 ones;

        const mfxU8 sb64Cols = (ALIGN_POWER_OF_TWO(framePar.modeInfoCols, MI_BLOCK_SIZE_LOG2)) >> MI_BLOCK_SIZE_LOG2;
        while ((MAX_TILE_WIDTH_B64 << minLog2TileCols) < sb64Cols)
        {
            minLog2TileCols ++;
        }
        while ((sb64Cols >> maxLog2TileCols) >= MIN_TILE_WIDTH_B64)
        {
            maxLog2TileCols ++;
        }
        maxLog2TileCols--;

        ones = framePar.log2TileCols - minLog2TileCols;
        while (ones--)
        {
            WriteBit(localBuf, 1);
        }
        if (framePar.log2TileCols < maxLog2TileCols)
        {
            WriteBit(localBuf, 0);
        }

        WriteBit(localBuf, framePar.log2TileRows != 0);
        if (framePar.log2TileRows != 0)
        {
            WriteBit(localBuf, framePar.log2TileRows != 1);
        }

        offsets.BitOffsetForFirstPartitionSize = (mfxU16)localBuf.bitOffset;;

        // size of compressed header (unknown so far, will be written by driver/HuC)
        WriteLiteral(localBuf, 0, 16);

        return localBuf.bitOffset;
    };

    mfxU16 PrepareFrameHeader(VP9MfxVideoParam const &par,
        mfxU8 *pBuf,
        mfxU32 bufferSizeBytes,
        Task const& task,
        VP9SeqLevelParam const &seqPar,
        BitOffsets &offsets)
    {
        if (bufferSizeBytes < VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE)
        {
            return 0; // zero size of header - indication that something went wrong
        }

        BitBuffer localBuf;
        localBuf.pBuffer = pBuf;
        localBuf.bitOffset = 0;

        mfxExtCodingOptionVP9 &opt = GetExtBufferRef(par);
        mfxU16 ivfHeaderSize = 0;

        if (opt.WriteIVFHeaders != MFX_CODINGOPTION_OFF)
        {
            if (task.m_insertIVFSeqHeader)
            {
                AddSeqHeader(par.mfx.FrameInfo.Width,
                    par.mfx.FrameInfo.Height,
                    par.mfx.FrameInfo.FrameRateExtN,
                    par.mfx.FrameInfo.FrameRateExtD,
                    opt.NumFramesForIVF,
                    localBuf.pBuffer);
                ivfHeaderSize += IVF_SEQ_HEADER_SIZE_BYTES;
            }

            AddPictureHeader(localBuf.pBuffer + ivfHeaderSize);
            ivfHeaderSize += IVF_PIC_HEADER_SIZE_BYTES;
        }

        localBuf.bitOffset += ivfHeaderSize * 8;

        mfxU16 totalBitsWritten = WriteUncompressedHeader(localBuf,
            task,
            seqPar,
            offsets);

        return (totalBitsWritten + 7) / 8;
    }
} // MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
