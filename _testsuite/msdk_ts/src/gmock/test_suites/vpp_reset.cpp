/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016-2018 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_reset
{

class TestSuite : protected tsVideoVPP, public tsSurfaceProcessor
{
public:
    TestSuite()
        : tsVideoVPP()
    {
        m_surf_in_processor = this;
        m_par.IOPattern     = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        m_par.AsyncDepth    = 1;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct do_not_use
    {
        mfxU32 alg_num;
        mfxU32 alg_list[6];
    };

    enum
    {
        RESET = 1,
        INIT,
    };

private:

    tsNoiseFiller m_noise;

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        do_not_use dnu_struct;
        HWType platform;
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
        m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

        return MFX_ERR_NONE;
    }

    bool isTcToSkip(const tc_struct& tc, tsExtBufType<mfxVideoParam>& par);
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode,      MFX_DEINTERLACING_BOB},

            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    80},
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  60},
        },
        {}
    },
    {/*01*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode,      MFX_DEINTERLACING_BOB},
        },
        {3, {MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE}}
    },
    {/*02*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode,      MFX_DEINTERLACING_BOB},

            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    80},
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  60},
        },
        {1, {MFX_EXTBUFF_VPP_PROCAMP}}
    },
    {/*03*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  50},
            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    40},
            {RESET, &tsStruct::mfxExtVPPProcAmp.Brightness,     30},
            {RESET, &tsStruct::mfxExtVPPDeinterlacing.Mode,     MFX_DEINTERLACING_BOB},
        },
    },
    {/*04*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     496},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      736},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    496},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     736},
        },
        {}
    },
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     496},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      736},
        },
        {}
    },
    {/*06*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    496},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     736},
        },
        {}
    },
    {/*07*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     464},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      704},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    464},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     704},
        },
        {}
    },
    {/*08*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     464},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      704},
        },
        {}
    },
    {/*09*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    464},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     704},
        },
        {}
    },
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {INIT, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.Type,         MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,   5},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.Type,        MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface,  5},

            {RESET, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface, 6},
        },
        {}
    },
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {INIT, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.Type,         MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,   5},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.Type,        MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface,  5},

            {RESET, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,  6},
        },
        {}
    },
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {INIT, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.Type,         MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,   5},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.Type,        MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface,  5},

            {RESET, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface, 4},
        },
        {}
    },
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {INIT, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.Type,         MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,   5},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.Type,        MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET},
            {INIT, &tsStruct::mfxExtOpaqueSurfaceAlloc.Out.NumSurface,  5},

            {RESET, &tsStruct::mfxExtOpaqueSurfaceAlloc.In.NumSurface,  4},
        },
        {}
    },
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.IOPattern,         MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        },
        {}
    },
    {/*15*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.IOPattern,         MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        },
        {}
    },
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.IOPattern,         MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        },
        {}
    },
    {/*17*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.IOPattern,         MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        },
        {}
    },
    {/*18*/ MFX_ERR_NONE,
        {
            {INIT,  &tsStruct::mfxVideoParam.AsyncDepth,        5},
            {RESET, &tsStruct::mfxVideoParam.AsyncDepth,        1},
        },
        {}
    },
    {/*19*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {INIT,  &tsStruct::mfxVideoParam.AsyncDepth,        1},
            {RESET, &tsStruct::mfxVideoParam.AsyncDepth,        5},
        },
        {}
    },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_YV12},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV420},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0},
        },
        {}
    },
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,     MFX_FOURCC_RGB3},
        },
        {}
    },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,    MFX_FOURCC_RGB3},
        },
        {}
    },
    {/*23*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,          MFX_FOURCC_RGB4},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV444},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0},
        },
        {}
    },
    {/*24*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_AYUV},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV444 },
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   8 },
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 8 },
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0 },
        },
        {},
    },
    {/*25*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_Y210},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          1},
        },
        {},
        MFX_HW_ICL
    },
    {/*26*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,    MFX_FOURCC_Y410},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV444},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0},
        },
        {},
        MFX_HW_ICL
    },
    {/*27*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_YUY2},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0},
        },
        {},
    },
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_UYVY},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0},
        },
        {},
    },
    {/*29*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_P010},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV420},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          1},
        },
        {},
    },
    {/*30*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_YV12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV420},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0},
        },
        {}
    },
    {/*31*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,          MFX_FOURCC_RGB4},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV444},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0},
        },
        {}
    },
    {/*32*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_AYUV},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat,   MFX_CHROMAFORMAT_YUV444 },
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma,   8 },
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 8 },
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Shift,          0 },
        },
        {},
    },
    {/*33*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_Y210},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          1},
        },
        {},
        MFX_HW_ICL
    },
    {/*34*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_Y410},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV444},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0},
        },
        {},
        MFX_HW_ICL
    },
    {/*35*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_YUY2},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0},
        },
        {},
    },
    {/*36*/
#if !defined(_WIN32)
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
#else
        MFX_ERR_INVALID_VIDEO_PARAM,
#endif
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_UYVY},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 8},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          0},
        },
        {},
    },
    {/*37*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_P010},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV420},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          1},
        },
        {},
    },
    {/*38*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_P016},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV420},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          1},
        },
        {},
        MFX_HW_TGL
    },
    {/*39*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_Y216},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV422},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          1},
        },
        {},
        MFX_HW_TGL
    },
    {/*40*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.FourCC,         MFX_FOURCC_Y416},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat,   MFX_CHROMAFORMAT_YUV444},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthLuma,   12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 12},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Shift,          1},
        },
        {},
        MFX_HW_TGL
    },
#if !defined(_WIN32)
    {/*41*/ MFX_ERR_NONE,
        {
            {INIT,  &tsStruct::mfxExtVPPFieldProcessing.Mode,     MFX_VPP_COPY_FRAME},

            {RESET, &tsStruct::mfxExtVPPFieldProcessing.Mode,     MFX_VPP_COPY_FIELD},
            {RESET, &tsStruct::mfxExtVPPFieldProcessing.InField,  MFX_PICSTRUCT_FIELD_TFF},
            {RESET, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICSTRUCT_FIELD_BFF},
        },
    },
#endif
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

void SetParamsDoNotUse(tsExtBufType<mfxVideoParam>& par, const TestSuite::do_not_use dnu)
{
    if(dnu.alg_num != 0)
    {
        mfxExtVPPDoNotUse* vpp_dnu;
        par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
        vpp_dnu = (mfxExtVPPDoNotUse*)par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
        vpp_dnu->NumAlg = dnu.alg_num;
        vpp_dnu->AlgList = new mfxU32[dnu.alg_num];

        memset(vpp_dnu->AlgList, 0, sizeof(mfxU32)*vpp_dnu->NumAlg);

        for(mfxU32 i = 0; i < dnu.alg_num; i++)
            vpp_dnu->AlgList[i] = dnu.alg_list[i];
    }
}

void CreateEmptyBuffers(tsExtBufType<mfxVideoParam>& par, tsExtBufType<mfxVideoParam>& pattern)
{
    for (mfxU32 i = 0; i < pattern.NumExtParam; i++)
    {
        if (pattern.ExtParam[i]->BufferId != MFX_EXTBUFF_VPP_DONOTUSE)
            par.AddExtBuffer(pattern.ExtParam[i]->BufferId, pattern.ExtParam[i]->BufferSz);
    }
}

bool TestSuite::isTcToSkip(const tc_struct& tc, tsExtBufType<mfxVideoParam>& par)
{
    if (par.vpp.In.FourCC == MFX_FOURCC_RGB4)
    {
        for (auto& sp : tc.set_par)
        {
            if (!sp.f)
                break;

            if (   sp.f->name.find("mfxExtVPPDenoise") != std::string::npos
                || sp.f->name.find("mfxExtVPPDetail") != std::string::npos
                || sp.f->name.find("mfxExtVPPProcAmp") != std::string::npos)
            {
                g_tsLog << "Filter incompatible with RGB - skip test case\n";
                return true;
            }
        }
    }

    if (   tc.sts == MFX_ERR_NONE
        || !CompareParams<const tc_struct&, decltype(RESET), decltype(par)>(tc, par, RESET, LOG_NOTHING))
        return false;

    g_tsLog << "Init parameters not changed - skip test case\n";
    return true;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    tsExtBufType<mfxVideoParam> def(m_par);

    SETPARS(&m_par, INIT);

    if (isTcToSkip(tc, m_par))
        throw tsSKIP;

    MFXInit();

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    mfxExtOpaqueSurfaceAlloc* pOSA = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

    if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        QueryIOSurf();

        m_request[0].NumFrameMin = m_request[0].NumFrameSuggested = pOSA->In.NumSurface;
        m_request[1].NumFrameMin = m_request[1].NumFrameSuggested = pOSA->Out.NumSurface;
    }

    if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        m_pSurfPoolIn->AllocOpaque(m_request[0], *pOSA);
    if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        m_pSurfPoolOut->AllocOpaque(m_request[1], *pOSA);

    g_tsStatus.expect(MFX_ERR_NONE);
    Init(m_session, &m_par);

    tsExtBufType<mfxVideoParam> par_init (def);
    CreateEmptyBuffers(*&par_init, *&m_par);

    g_tsStatus.expect(MFX_ERR_NONE);
    GetVideoParam(m_session, &par_init);

    tsExtBufType<mfxVideoParam> par_reset (def);
    SETPARS(&par_reset, RESET);
    SetParamsDoNotUse(*&par_reset, tc.dnu_struct);

    //adjust expected status for
    mfxStatus expected = tc.sts;
    if (tc.platform != MFX_HW_UNKNOWN && g_tsHWtype < tc.platform)
        expected = MFX_ERR_INVALID_VIDEO_PARAM;

    g_tsStatus.expect(expected);
    Reset(m_session, &par_reset);

    if (MFX_ERR_NONE == expected) // GetVideoParam checks are valid only in case of succesfull reset
    {
        EXPECT_EQ(m_par, par_init) << "ERROR: Init parameters and parameters from GetVideoParams are not equal\n";

        tsExtBufType<mfxVideoParam> par_after_reset (def);
        CreateEmptyBuffers(*&par_after_reset, *&par_init);
        CreateEmptyBuffers(*&par_after_reset, *&par_reset);

        g_tsStatus.expect(MFX_ERR_NONE);
        GetVideoParam(m_session, &par_after_reset);

        mfxExtVPPDoNotUse* dnu = (mfxExtVPPDoNotUse*)par_reset.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

        for (mfxU32 i = 0; i< par_init.NumExtParam; i++)
        {
            mfxExtBuffer* reset = 0;
            mfxExtBuffer* after_reset = 0;

            reset       = par_reset.GetExtBuffer(      par_init.ExtParam[i]->BufferId);
            after_reset = par_after_reset.GetExtBuffer(par_init.ExtParam[i]->BufferId);

            // Creating empty buffer
            def.AddExtBuffer(par_init.ExtParam[i]->BufferId, par_init.ExtParam[i]->BufferSz);
            mfxExtBuffer* empty = def.GetExtBuffer(par_init.ExtParam[i]->BufferId);

            bool in_dnu = false;
            if (dnu != 0)
            {
                for (mfxU32 j = 0; j < dnu->NumAlg; j++)
                {
                    if (par_init.ExtParam[i]->BufferId == dnu->AlgList[j])
                    {
                        in_dnu = true;
                        break;
                    }
                }
            }

            EXPECT_FALSE(in_dnu && (0 != memcmp(empty, after_reset, std::max(empty->BufferSz, after_reset->BufferSz))))
                << "ERROR: Filter from Init was disabled in Reset but exists after Reset \n";

            EXPECT_FALSE(reset != 0 && (0 != memcmp(reset, after_reset, std::max(reset->BufferSz, after_reset->BufferSz))) )
                << "ERROR: Filter from Init was changed in Reset but parameters from Reset and after Reset are not equal \n";

            EXPECT_FALSE(!in_dnu && reset == 0 && (0 != memcmp(par_init.ExtParam[i], after_reset, std::max(par_init.ExtParam[i]->BufferSz, after_reset->BufferSz))))
                << "ERROR: Filter's parameters from Init and after Reset are not equal \n";
        }

        for (mfxU32 i = 0; i< par_reset.NumExtParam; i++)
        {
            if (par_reset.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DONOTUSE) continue; // VPP should not configure mfxExtVPPDoNotUse buffer!

            mfxExtBuffer* after_reset = 0;
            after_reset = par_after_reset.GetExtBuffer(par_reset.ExtParam[i]->BufferId);

            EXPECT_FALSE (after_reset == 0) << "ERROR: Filter specified in Reset does not exists after Reset \n";

            EXPECT_FALSE(0 != memcmp(par_reset.ExtParam[i], after_reset, std::max(par_reset.ExtParam[i]->BufferSz, after_reset->BufferSz)))
                << "ERROR: Filter's parameters from Reset and after Reset are not equal \n";
        }

        if (dnu && dnu->AlgList) delete[] dnu->AlgList;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_reset);
}

namespace vpp_8b_420_yv12_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_YV12;
            m_par.vpp.In.BitDepthLuma = 8;
            m_par.vpp.In.BitDepthChroma = 8;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

            m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
            m_par.vpp.Out.BitDepthLuma = 8;
            m_par.vpp.Out.BitDepthChroma = 8;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_420_yv12_reset);
}

namespace vpp_8b_422_uyvy_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_UYVY;
            m_par.vpp.In.BitDepthLuma = 8;
            m_par.vpp.In.BitDepthChroma = 8;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC = MFX_FOURCC_UYVY;
            m_par.vpp.Out.BitDepthLuma = 8;
            m_par.vpp.Out.BitDepthChroma = 8;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_422_uyvy_reset);
}

namespace vpp_8b_422_yuy2_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_YUY2;
            m_par.vpp.In.BitDepthLuma = 8;
            m_par.vpp.In.BitDepthChroma = 8;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC = MFX_FOURCC_YUY2;
            m_par.vpp.Out.BitDepthLuma = 8;
            m_par.vpp.Out.BitDepthChroma = 8;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_422_yuy2_reset);
}

namespace vpp_8b_444_ayuv_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_AYUV;
            m_par.vpp.In.BitDepthLuma = 8;
            m_par.vpp.In.BitDepthChroma = 8;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC = MFX_FOURCC_AYUV;
            m_par.vpp.Out.BitDepthLuma = 8;
            m_par.vpp.Out.BitDepthChroma = 8;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_444_ayuv_reset);
}

namespace vpp_8b_444_rgb4_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_RGB4;
            m_par.vpp.In.BitDepthLuma = 8;
            m_par.vpp.In.BitDepthChroma = 8;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC = MFX_FOURCC_RGB4;
            m_par.vpp.Out.BitDepthLuma = 8;
            m_par.vpp.Out.BitDepthChroma = 8;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_8b_444_rgb4_reset);
}

namespace vpp_10b_420_p010_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_P010;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 1;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

            m_par.vpp.Out.FourCC = MFX_FOURCC_P010;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 1;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_420_p010_reset);
}

namespace vpp_10b_422_y210_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_Y210;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 1;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV422;

            m_par.vpp.Out.FourCC = MFX_FOURCC_Y210;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 1;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_422_y210_reset);
}

namespace vpp_10b_444_y410_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC = MFX_FOURCC_Y410;
            m_par.vpp.In.BitDepthLuma = 10;
            m_par.vpp.In.BitDepthChroma = 10;
            m_par.vpp.In.Shift = 0;
            m_par.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

            m_par.vpp.Out.FourCC = MFX_FOURCC_Y410;
            m_par.vpp.Out.BitDepthLuma = 10;
            m_par.vpp.Out.BitDepthChroma = 10;
            m_par.vpp.Out.Shift = 0;
            m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_10b_444_y410_reset);
}

namespace vpp_12b_420_p016_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = MFX_FOURCC_P016;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = 12;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = 12;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = 1;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_12b_420_p016_reset);
}

namespace vpp_12b_422_y216_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = MFX_FOURCC_Y216;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = 12;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = 12;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = 1;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV422;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_12b_422_y216_reset);
}

namespace vpp_12b_444_y416_reset
{
    class TestSuite : public vpp_reset::TestSuite
    {
    public:
        TestSuite() : vpp_reset::TestSuite()
        {
            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = MFX_FOURCC_Y416;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = 12;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = 12;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = 1;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
        }
    };
    TS_REG_TEST_SUITE_CLASS(vpp_12b_444_y416_reset);
}