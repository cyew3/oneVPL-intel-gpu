/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: slice_size_reporting.cpp

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_struct.h"

namespace slice_size_reporting
{

class BitstreamChecker : public tsBitstreamProcessor
{
    private:
        mfxU32 slice_size[2];

    public:
        BitstreamChecker(mfxU32 _slice_size_w, mfxU32 _slice_size_h)
        {
            slice_size[0] = _slice_size_w;
            slice_size[1] = _slice_size_h;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);


};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxExtEncodedSlicesInfo *buff = NULL;
    for(mfxU16 i = 0; i < bs.NumExtParam; i++)
    {
        if (bs.ExtParam[i]->BufferId == MFX_EXTBUFF_ENCODED_SLICES_INFO)
            buff = (mfxExtEncodedSlicesInfo*)bs.ExtParam[i];
    }
    if (buff)
    {
        if ( (buff->SliceSize) &&
            ((buff->SliceSize[0] == slice_size[0]) || (buff->SliceSize[1] == slice_size[1])))//expect slice size  equal reported
        {
            return MFX_ERR_NONE;
        }
        else
        {
            g_tsLog << "ERROR: Reported slice size is wrong!\nExpected: w = " << slice_size[0] << ", h = " << slice_size[1] << ";  Real: w = " << buff->SliceSize[0] << ", h = " << buff->SliceSize[1] << "\n";
            return MFX_ERR_ABORTED;
        }
    }
    else
    {
        g_tsLog << "ERROR: Buffer ENCODED_SLICES_INFO not found!\n";
        return MFX_ERR_ABORTED;
    }
    return MFX_ERR_NONE;
}

int test(mfxU32 codecId)
{
    TS_START;

    //int encoder
    tsVideoEncoder enc(codecId);

    //set input stream
    const char* stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
    g_tsStreamPool.Reg();
    tsRawReader reader = tsRawReader(stream, enc.m_par.mfx.FrameInfo);
    enc.m_filler = (tsSurfaceProcessor*)(&reader);
    int exp_slice_size[2] = {720, 480};

    //set ext buff in bs
    mfxExtEncodedSlicesInfo buff;
    buff.Header.BufferId = MFX_EXTBUFF_ENCODED_SLICES_INFO;
    buff.Header.BufferSz = sizeof(mfxExtEncodedSlicesInfo);
    mfxExtBuffer *tmp = ((mfxExtBuffer*)(&buff));
    enc.m_bitstream.NumExtParam = 1;
    enc.m_bitstream.ExtParam = &tmp;

    //set bs checker
    BitstreamChecker bs_check(exp_slice_size[0], exp_slice_size[1]);
    enc.m_bs_processor = &bs_check;


    //defoult pipeline
    enc.EncodeFrames(1);

    TS_END;
    return 0;
}

int AVCTest     (unsigned int) { return test(MFX_CODEC_AVC); }
int MPEG2Test   (unsigned int) { return test(MFX_CODEC_MPEG2); }
int HEVCTest    (unsigned int) { return test(MFX_CODEC_HEVC); }


TS_REG_TEST_SUITE(hevce_slice_size_report, HEVCTest, 1);
TS_REG_TEST_SUITE(avce_slice_size_report, AVCTest, 1);
TS_REG_TEST_SUITE(mpeg2e_slice_size_report, MPEG2Test, 1);
}