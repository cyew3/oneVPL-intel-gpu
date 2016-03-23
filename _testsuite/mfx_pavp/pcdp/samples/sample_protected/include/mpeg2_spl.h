//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef _MPEG2_SPL_H__
#define _MPEG2_SPL_H__

#include <vector>
#include <memory>

#include "abstract_splitter.h"

#include "mpeg2_dec_bstream.h"

namespace ProtectedLibrary
{
#define NUM_REST_BYTES 4

#define PICTURE_START_CODE             0x00000100
#define SEQUENCE_HEADER_CODE           0x000001B3
#define EXTENSION_START_CODE           0x000001B5
#define SEQUENCE_EXTENSION_ID          0x00000001
#define SEQUENCE_SCALABLE_EXTENSION_ID 0x00000005
#define DATA_PARTITIONING              0x00000000
#define PICTURE_CODING_EXTENSION_ID    0x00000008

#define FRAME_PICTURE            3
#define TOP_FIELD                1
#define BOTTOM_FIELD             2

class MPEG2_Spl : public AbstractSplitter
{

public:

    MPEG2_Spl();

    virtual ~MPEG2_Spl();

    virtual mfxStatus Reset();

    virtual mfxStatus GetFrame(mfxBitstream * bs_in, FrameSplitterInfo ** frame);

    virtual mfxStatus PostProcessing(FrameSplitterInfo *frame, mfxU32 sliceNum);

    void ResetCurrentState();

protected:

    mfxStatus Init();

    mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out);
    mfxStatus PrepareBuffer();
    mfxStatus FindSlice(mfxU32 *code);
    mfxStatus ParseBsOnSlices();
    mfxStatus DecodeSliceHeader(mfxU32 code);

    void Close();

    VideoBs  Video;
    bool m_bFirstField;
    mfxU8 *m_first_slice_ptr;
    mfxI32 m_width;
    mfxI32 m_height;

    mfxI32 extension_start_code_ID;
    mfxI32 scalable_mode;
    mfxU32 picture_structure;

    std::vector<SliceSplitterInfo>  m_slices;
    FrameSplitterInfo m_frame;
    mfxU8 m_last_bytes[NUM_REST_BYTES];
    mfxBitstream m_bs;
    mfxU32 first_slice_DataOffset;

    mp2_VLCTable  vlcMBAdressing;

    struct FcState
    {
        enum { NONE = 0, TOP = 1, BOT = 2, FRAME = 3 };
        mfxU8 picStart : 1;
        mfxU8 picHeader : 2;
    } m_fcState;

    void ResetFcState(FcState& state) { state.picStart = state.picHeader = 0; }
};

} // namespace ProtectedLibrary

#endif // _MPEG2_SPL_H__
