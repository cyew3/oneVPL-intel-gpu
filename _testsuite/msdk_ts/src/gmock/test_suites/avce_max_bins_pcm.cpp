/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"


//#define DEBUG_STREAM "avce_max_bins_pcm.264"

namespace avce_max_bins_pcm
{

class SFiller : public tsSurfaceProcessor
{
private:
    mfxU32 m_c;
public:
    SFiller() : m_c(0){ srand(0); };
    mfxStatus ProcessSurface(mfxFrameSurface1& s);
};

 mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
 {
    tsFrame d(s);
    mfxU32 mh = s.Info.Height / 10;
    mfxU32 mw = s.Info.Width / 6 * (1 + !(m_c++ % 10));

    if (m_c % 100 < 4)
    {
        mh = s.Info.Height;
        mw = s.Info.Width/4;
    }

    for(mfxU32 h = 0; h < s.Info.Height; h++)
    {
        for(mfxU32 w = 0; w < s.Info.Width; w++)
        {
            d.Y(w,h) = 16;
            d.U(w,h) = 127;
            d.V(w,h) = 127;
        }
    }

    for(mfxU32 w = 0; w < mw; w++)
    {
        for(mfxU32 h = 0; h < mh; h++)
        {
            d.Y(w, h) = (rand() % (235 - 16)) + 16;
            d.U(w, h) = (rand() % (235 - 16)) + 16;
            d.V(w, h) = (rand() % (235 - 16)) + 16;
        }
    }

     return MFX_ERR_NONE;
 }

using namespace BS_AVC2;

class BitstreamChecker : public tsBitstreamProcessor, public tsParserAVC2
{
public:
#ifdef DEBUG_STREAM
    tsBitstreamWriter m_bsw;
#endif

    BitstreamChecker()
        : tsParserAVC2( INIT_MODE_PARSE_SD
#if (!defined(LINUX32) && !defined(LINUX64))
                       //use slice threading w/ auto-sync at new AU start
                       //disable for linux due to issue in old gcc versions with std threads
                       |INIT_MODE_MT_SD
#endif
                      )
#ifdef DEBUG_STREAM
    , m_bsw(DEBUG_STREAM)
#endif
    {
        set_trace_level(0);
    }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};


const Bs8u SubWidthC[5]  = {1,2,2,1,1};
const Bs8u SubHeightC[5] = {1,2,1,1,1};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;

    if (bs.Data)
        SetBuffer(bs);

    bool check_max_bins = true;

    //driver supports PCMs now
#if 0
    if (g_tsImpl != MFX_IMPL_SOFTWARE)
        check_max_bins = false; // parse + dump info only
#endif

    while(checked++ < nFrames)
    {
        mfxU64 BinCountsInNALunits  = 0;
        mfxU64 NumBytesInVclNALunits = 0;
        mfxU64 RawMbBits = 0;
        mfxU64 PicSizeInMbs = 0;
        mfxU32 max_bits_per_mb_denom = 1;
        mfxU32 nSlice = 0;

        UnitType& hdr = ParseOrDie();

        for(Bs32u i = 0; i < hdr.NumUnits; i++)
        {
            auto nalu = hdr.nalu[i];

            if(    nalu->nal_unit_type != SLICE_IDR
                && nalu->nal_unit_type != SLICE_NONIDR)
                continue;

            auto&   s  = *nalu->slice;
            auto&  sps = *s.pps->sps;

            mfxU16 MbWidthC  = 16 / SubWidthC[sps.chroma_format_idc + sps.separate_colour_plane_flag];
            mfxU16 MbHeightC = 16 / SubHeightC[sps.chroma_format_idc + sps.separate_colour_plane_flag];
            mfxU16 BitDepthY = 8 + sps.bit_depth_luma_minus8;
            mfxU16 BitDepthC = 8 + sps.bit_depth_chroma_minus8;

            BinCountsInNALunits   += s.BinCount;
            NumBytesInVclNALunits += nalu->NumBytesInNalUnit - 3 - (nalu == hdr.nalu[0]);
            RawMbBits = 256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC;
            PicSizeInMbs = (sps.pic_width_in_mbs_minus1 + 1) * (( 2 - sps.frame_mbs_only_flag ) * (sps.pic_height_in_map_units_minus1 + 1) / ( 1 + s.field_pic_flag ));

            g_tsLog << "Slice #" << nSlice++ << ": BinCount = " << s.BinCount << "; NumBytesInNALunit = " << nalu->NumBytesInNalUnit << ";\n";

            if(sps.vui_parameters_present_flag && sps.vui->bitstream_restriction_flag)
            {
                max_bits_per_mb_denom = sps.vui->max_bits_per_mb_denom;
            }

            for(mfxU32 i = 0; i < s.NumMB; i++)
            {
                auto& mb = s.mb[i];

                if(sps.profile_idc == 100)
                {
                    if (mb.NumBits > 128 + RawMbBits / max_bits_per_mb_denom) {
                        if (check_max_bins) {
                            EXPECT_LE( mb.NumBits, 128 + RawMbBits / max_bits_per_mb_denom );
                            g_tsLog << "ERROR: ";
                        } else {
                            g_tsLog << "ERROR_SKIPPED: ";
                        }
                        g_tsLog << "numBits in MB exceeds bound\n"
                                << "  FAILED at MB #" << mb.Addr << " (" << mb.x << ":" << mb.y <<")";
                    }
                } else
                {
                    if (mb.NumBits > 3200) {
                        if (check_max_bins) {
                            EXPECT_LE( mb.NumBits, 3200 );
                            g_tsLog << "ERROR: ";
                        } else {
                            g_tsLog << "ERROR_SKIPPED: ";
                        }
                        g_tsLog << "numBits in MB exceeds bound\n" << "  FAILED at MB #" << mb.Addr;
                    }
                }
            }
        }
        if (BinCountsInNALunits > 32 * NumBytesInVclNALunits / 3 + (RawMbBits*PicSizeInMbs) / 32) {
            if (check_max_bins) {
                EXPECT_LE( BinCountsInNALunits, 32 * NumBytesInVclNALunits / 3 + (RawMbBits*PicSizeInMbs) / 32 );
                g_tsLog << "ERROR: ";
            } else {
                g_tsLog << "ERROR_SKIPPED: ";
            }
            g_tsLog << "BinCountsInNALunits in MB exceeds bound\n";
        }
        g_tsLog << "BinCountsInNALunits <= 32 * NumBytesInVclNALunits / 3 + (RawMbBits*PicSizeInMbs) / 32  : "
                <<  BinCountsInNALunits << " <= 32 * " << NumBytesInVclNALunits << " / 3 + (" << RawMbBits << "*" << PicSizeInMbs << ") / 32  : "
                <<  BinCountsInNALunits << " <= " << (32 * NumBytesInVclNALunits / 3 + (RawMbBits*PicSizeInMbs) / 32) << "\n";
    }

#ifdef DEBUG_STREAM
    m_bsw.ProcessBitstream(bs, nFrames);
#endif

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

int test(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    SFiller sf;
    BitstreamChecker c;
    enc.m_filler = &sf;
    enc.m_bs_processor = &c;

    mfxInfoMFX& mfx = enc.m_par.mfx;
    mfxExtCodingOption& co = enc.m_par;

    mfx.FrameInfo.Width  = 1920;
    mfx.FrameInfo.Height = 1088;
    mfx.FrameInfo.CropW  = 1920;
    mfx.FrameInfo.CropH  = 1080;
    mfx.FrameInfo.FrameRateExtN = 24000;
    mfx.FrameInfo.FrameRateExtD = 1001;
    mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    mfx.CodecProfile        = 100;
    mfx.CodecLevel          = 41;
    mfx.NumThread           = 0;
    mfx.TargetUsage         = 4;
    mfx.GopPicSize          = 24;

    if (mfx.LowPower == MFX_CODINGOPTION_ON)
        mfx.GopRefDist = 1;
    else
        mfx.GopRefDist = 3;

    mfx.GopOptFlag          = 1;
    mfx.IdrInterval         = 0;
    mfx.RateControlMethod   = MFX_RATECONTROL_VBR;
    mfx.InitialDelayInKB    = 0;
    mfx.TargetKbps          = 49900;
    mfx.MaxKbps             = 50000;
    mfx.BufferSizeInKB      = 3750;
    mfx.NumSlice            = 4;
    mfx.NumRefFrame         = 1;
    co.CAVLC                = 32;
    co.RecoveryPointSEI     = 16;
    co.SingleSeiNalUnit     = 32;
    co.VuiVclHrdParameters  = 16;
    co.AUDelimiter          = 16;
    co.EndOfStream          = 32;
    co.PicTimingSEI         = 16;
    co.VuiNalHrdParameters  = 16;

    enc.EncodeFrames(g_tsConfig.sim ? 10 : 300);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_max_bins_pcm, test, 1);
};
