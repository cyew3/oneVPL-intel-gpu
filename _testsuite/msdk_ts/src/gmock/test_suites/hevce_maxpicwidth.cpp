/******************************************************************************* *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
Description: This test suite validates a PRD requirement for HEVC legacy encoder.
This requirement states that HEVC legacy encoder must correctly encode bit streams
with a 2160p resolution on Skylake platforms. Resolution 2160p implies a progressive
bit stream with resolution 3840x2160.

Algorithm:
- MFXInit
- Load HEVC plugin for encoder
- Init encoder resources
- EncodeFrames
- Create decoder
- DecodeFrames with PSNR verification
- Close decoder and encoder
*/

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_maxpicwidth {

//#define MANUAL_DEBUG_MODE

constexpr mfxF64 PSNR_THRESHOLD = 30.0;

class TestSuite
    : public tsVideoEncoder
    , public tsBitstreamProcessor
{
    tsRawReader* m_srfReader; // initial stream reader
    const char* m_stream; // initial stream
    mfxFrameInfo m_rawReaderFrameInfo; // initial stream parameters

    mfxBitstream m_bst; // duplicate of m_bitstream
    static constexpr mfxU32 m_nFrames = 10; // number of encoded/decoded frames

#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter* m_bsWriter; // encoded video stream writer
#endif

public:

    static constexpr mfxU16 n_cases = 1;

    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , m_stream(g_tsStreamPool.Get("YUV/horsecab_3840x2160_30fps_1s_YUV.yuv"))
    {
        // Set parameters for further encoding
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 3840;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 2160;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 35;
        m_par.mfx.GopPicSize = 11;
        m_par.mfx.GopRefDist = 1;

        // Set parameters for m_filler
        m_rawReaderFrameInfo.Width = m_rawReaderFrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_rawReaderFrameInfo.Height = m_rawReaderFrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_rawReaderFrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_rawReaderFrameInfo.FourCC = MFX_FOURCC_YV12;
        m_srfReader = new tsRawReader(m_stream, m_rawReaderFrameInfo);
        g_tsStreamPool.Reg();
        m_filler = m_srfReader;

        m_bs_processor = this;

        // Initialize m_bst to store the encoded bit stream
        // Memory sufficient for a raw video stream of the given resolution with chroma format 4:2:0 with m_nFrames:
        m_bst.MaxLength = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 / 2 * m_nFrames; 
        m_bst.Data = new mfxU8[m_bst.MaxLength];
        memset(m_bst.Data,0,m_bst.MaxLength);
        m_bst.DataOffset = 0;
        m_bst.DataLength = 0;

#ifdef MANUAL_DEBUG_MODE
        m_bsWriter = new tsBitstreamWriter("hevce_maxpicwidth.h265");
#endif
    }

    ~TestSuite()
    {
        if (m_srfReader) delete m_srfReader;
        if (m_bst.Data)  delete[] m_bst.Data;
#ifdef MANUAL_DEBUG_MODE
        if (m_bsWriter)  delete m_bsWriter;
#endif
    }

    mfxI32 RunTest(mfxU16 id);

private:
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        using namespace BS_HEVC;
        if (bs.Data)
        {
            // Copy m_bitstream.Data to m_bst.Data to get access to the encoded bit stream even after it had been processed
            if (m_bst.MaxLength >= m_bst.DataLength + bs.DataLength)
            {
                memcpy(&m_bst.Data[m_bst.DataLength], &bs.Data[bs.DataOffset], bs.DataLength);
                m_bst.DataLength += bs.DataLength;
            }
            else
                return MFX_ERR_ABORTED;
#ifdef MANUAL_DEBUG_MODE
            // Output the encoded bit stream
            m_bsWriter->ProcessBitstream(bs, nFrames);
#endif
	}
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

}; // end of class TestSuite

// Surface Processor for Decoder
class PSNRVerifier : public tsSurfaceProcessor
{
    tsRawReader* m_srfReader; // initial stream reader
    mfxFrameSurface1 m_refSrf; // surfaces of the initial stream
    mfxU32 m_frameNumber; // frame counter
#ifdef MANUAL_DEBUG_MODE
    tsSurfaceWriter* m_srfWriter; // decoded stream writer
#endif

public:
    PSNRVerifier(mfxFrameSurface1& s, mfxU16 nFrames, const char* stream, mfxFrameInfo rawReaderFrameInfo)
        : tsSurfaceProcessor()
    {
        m_srfReader = new tsRawReader(stream, rawReaderFrameInfo, nFrames);
        m_max = nFrames;
        m_refSrf = s;
        m_frameNumber = 0;
#ifdef MANUAL_DEBUG_MODE
        m_srfWriter = new tsSurfaceWriter("hevce_maxpicwidth.yuv", true); // append = true
#endif
    }

    ~PSNRVerifier()
    {
        if (m_srfReader) delete m_srfReader;
#ifdef MANUAL_DEBUG_MODE
        if (m_srfWriter) delete m_srfWriter;
#endif
    }

private:
    mfxStatus ProcessSurface(mfxFrameSurface1& s /* current decoder's surface */ )
    {
#ifdef MANUAL_DEBUG_MODE
        // Output current decoded surface
        m_srfWriter->ProcessSurface(s);
#endif
        // Read next surface from the initial video stream and put it into m_refSrf
        m_srfReader->ProcessSurface(m_refSrf);
        tsFrame ref_frame(m_refSrf); // initial frame
        tsFrame out_frame(s); // decoded frame
        mfxF64 psnr_y = PSNR(ref_frame, out_frame, 0);
        mfxF64 psnr_u = PSNR(ref_frame, out_frame, 1);
        mfxF64 psnr_v = PSNR(ref_frame, out_frame, 2);
        g_tsLog << "=================================================================================\n";
        g_tsLog << "PSNR for Y, U and V components: ( " << psnr_y << ", " << psnr_u << ", " << psnr_v << ") on frame #" << m_frameNumber << "\n";
        if ((psnr_y < PSNR_THRESHOLD) ||
            (psnr_u < PSNR_THRESHOLD) ||
            (psnr_v < PSNR_THRESHOLD))
        {
            g_tsLog << "ERROR: Low PSNR" << "\n";
            return MFX_ERR_ABORTED;
        }
        m_frameNumber++;
        return MFX_ERR_NONE;
    }

}; // end of class PSNRVerifier

mfxI32 TestSuite::RunTest(mfxU16 id)
{
    TS_START;

    if (g_tsOSFamily != MFX_OS_FAMILY_LINUX) {
        g_tsLog << "[SKIPPED] The test is for Linux OS only\n";
        throw tsSKIP;
    }

    if (g_tsImpl != MFX_IMPL_HARDWARE) {
        g_tsLog << "[SKIPPED] The test is for HW implementation only\n";
        throw tsSKIP;
    }

    if (g_tsHWtype != MFX_HW_SKL) {
        g_tsLog << "[SKIPPED] The test is for Skylake platform only\n";
        throw tsSKIP;
    }

    MFXInit();
    // Load HEVC plugin
    Load();
    Init();
    EncodeFrames(m_nFrames);

    tsVideoDecoder dec(MFX_CODEC_HEVC);
    // Initialize decoder's m_bs_processor with a new tsBitstreamReader object
    // which reads the encoded bit stream duplicated in TestSuite::m_bst
    tsBitstreamReader reader(m_bst, m_bst.DataLength);
    dec.m_bs_processor = &reader;

    dec.Init();
    // Get encoder's surface pool for further PSNR verification
    mfxFrameSurface1* ref = GetSurface();
    // Initialize decoder's m_surf_processor with a PSNRVerifier object
    PSNRVerifier v(*ref, m_nFrames, m_stream, m_rawReaderFrameInfo);
    dec.m_surf_processor = &v;

    dec.DecodeFrames(m_nFrames);

    dec.Close();
    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_maxpicwidth);

} // end of namespace hevce_maxpicwidth
