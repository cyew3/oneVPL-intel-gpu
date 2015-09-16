#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_rotate
{
class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
        EXTBUFF_VPP_ROTATION,
        ALLOC_OPAQUE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*01*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*02*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*03*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_IN_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*04*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*05*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*06*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*07*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_IN_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*08*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*09*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*10*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*11*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_IN_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*12*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*13*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*14*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*15*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*16*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*17*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*18*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*19*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*20*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*21*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180}}
    },
    {/*22*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*23*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*24*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*25*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*26*/ MFX_ERR_NONE, ALLOC_OPAQUE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_270}}
    },
    {/*27*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 0}}
    },
    {/*28*/
     //Invalid angle
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 100}}
    },
    {/*29*/
     //Invalid angle
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 360}}
    },
    {/*30*/
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*31*/
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*32*/
        MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*33*/
        MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*34*/
     //Unsupported FourCC
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*35*/
     //Unsupported FourCC
        MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV16},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV16},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    },
    {/*36*/
        MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422},
        {EXTBUFF_VPP_ROTATION, &tsStruct::mfxExtVPPRotation.Angle, 90}}
    }
};


const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    std::vector<mfxExtBuffer*> buffs;

    MFXInit();

    SETPARS(m_pPar, MFX_PAR);

    CreateAllocators();

    SetFrameAllocator();
    SetHandle();

    mfxExtVPPRotation ext_buf_rotation;

    memset(&ext_buf_rotation, 0, sizeof(mfxExtVPPRotation));
    ext_buf_rotation.Header.BufferId = MFX_EXTBUFF_VPP_ROTATION;
    ext_buf_rotation.Header.BufferSz = sizeof(mfxExtVPPRotation);

    SETPARS(&ext_buf_rotation, EXTBUFF_VPP_ROTATION);

    buffs.push_back((mfxExtBuffer*)&ext_buf_rotation);

    if (tc.mode == ALLOC_OPAQUE)
    {
        mfxExtOpaqueSurfaceAlloc osa;
        memset(&osa, 0, sizeof(mfxExtOpaqueSurfaceAlloc));
        osa.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
        osa.Header.BufferSz = sizeof(mfxExtOpaqueSurfaceAlloc);
        QueryIOSurf();

        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);

        buffs.push_back((mfxExtBuffer*)&osa);
    }

    m_par.NumExtParam = (mfxU16)buffs.size();
    m_par.ExtParam = &buffs[0];

    g_tsStatus.expect(tc.sts);
    if (!(m_impl & MFX_IMPL_VIA_D3D11) && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
        g_tsStatus.expect(MFX_WRN_FILTER_SKIPPED);

    if (m_par.vpp.Out.FourCC == MFX_FOURCC_YV12)  // yv12 on output is not supported in any case case
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);

    if ( (m_par.vpp.In.FourCC == MFX_FOURCC_NV16) || (m_par.vpp.Out.FourCC == MFX_FOURCC_NV16) ) // nv16 usage leads to SW fallback, where rotation cannot be used
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);

    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_rotate);

}
