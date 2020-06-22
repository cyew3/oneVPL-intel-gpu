/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2020 Intel Corporation. All Rights Reserved.

File Name: avce_et_mds_encode.cpp
\* ****************************************************************************** */

/*!
\file avce_et_mds_encode.cpp
\brief Gmock test avce_et_mds_encode

Description:
This suite tests AVC encoder

Algorithm:
- Call Query() of the MSDK session w/ EncTools
- Call Init() of the MSDK session w/ EncTools
- Call Encode() of the MSDK sessio w/ EncTools
- Check bitstream quality

*/
#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_struct.h"
#include "mfxenctools.h"
#include "ts_utils.h"
#include <fstream>

#define DUMP_BS(bs);  std::fstream fout("out.bin", std::fstream::binary | std::fstream::out);fout.write((char*)bs.Data, bs.DataLength); fout.close();

/*! \brief Main test name space */
namespace avce_et_mds_encode
{

    enum
    {
        MFX_PAR  = 1,
        ETC_PAR  = 2,
    };

    const size_t frames_num = 9;
    const mfxF64 PSNR_THRESHOLD = 30;

    /*!\brief Structure of test suite parameters */
    struct tc_struct
    {
        mfxStatus sts_query;
        mfxStatus sts_init;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    class Verifier : public tsSurfaceProcessor
    {
    public:
        tsRawReader* m_reader;
        mfxFrameSurface1 m_ref;
        mfxU32 m_frames;

        Verifier(mfxFrameSurface1& ref, mfxU16 nFrames, const char* sn)
            : tsSurfaceProcessor()
            , m_reader(0)
        {
            m_reader = new tsRawReader(sn, ref.Info, nFrames);
            m_max = nFrames;
            m_ref = ref;
            m_frames = 0;
        }

        ~Verifier()
        {
            if (m_reader)
            {
                delete m_reader;
                m_reader = NULL;
            }
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_reader->ProcessSurface(m_ref);

            tsFrame ref_frame(m_ref);
            tsFrame s_frame(s);
            mfxF64 psnr_y = PSNR(ref_frame, s_frame, 0);
            mfxF64 psnr_u = PSNR(ref_frame, s_frame, 1);
            mfxF64 psnr_v = PSNR(ref_frame, s_frame, 2);
            if ((psnr_y < PSNR_THRESHOLD) ||
                (psnr_u < PSNR_THRESHOLD) ||
                (psnr_v < PSNR_THRESHOLD))
            {
                g_tsLog << "ERROR: Low PSNR on frame " << m_frames << "\n";
                return MFX_ERR_ABORTED;
            }
            m_frames++;
            return MFX_ERR_NONE;
        }
    };


    class VideoEncoder :public tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        VideoEncoder() :tsVideoEncoder(MFX_CODEC_AVC) {}
        //! \brief A destructor
        ~VideoEncoder() {}
        //! \brief Initialize params common for whole test suite
        void initParams();
        //! \brief Initializes FrameAllocator
        /*! \details This method is required because of some troubles on Windows platform
        befoe Init() calling frameAllocator should be defined*/
        void allocatorInit();
        //! \brief Sets Handle
        /*! \details This method is required because of some troubles on Windows platform (working with d3d)
        before Init() calling Handle should be defined*/
        void setHandle();
    };

    //!\brief Main test class
    class TestSuite
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! Encoder
        VideoEncoder encoder;
    };

    void VideoEncoder::initParams()
    {
        //Default codec was set in encoder constructor
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            m_par.mfx.MaxKbps = 0;
        }
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_par.mfx.FrameInfo.Width  = 720;
        m_par.mfx.FrameInfo.Height = 480;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        m_par.AddExtBuffer(MFX_EXTBUFF_ENCTOOLS_CONFIG, sizeof(mfxExtEncToolsConfig));

    }

    void VideoEncoder::allocatorInit() {
        if (!m_pFrameAllocator
            && ((m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if (!GetAllocator())
            {
                if (m_pVAHandle)
                    SetAllocator(m_pVAHandle, true);
                else
                    UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
    }

    void VideoEncoder::setHandle()
    {
        mfxHDL hdl;
        mfxHandleType type;
        m_pFrameAllocator->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);


    }

    const tc_struct TestSuite::test_case[] =
    {
        //MDS
        //CQP + valid video params + all EncTools options ON
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CQP + valid video params + all EncTools options OFF
        {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CQP + valid video params + all EncTools options UNKNOWN
        {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + valid video params + all EncTools options ON
        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CBR + valid video params + all EncTools options OFF
        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CBR + valid video params + all EncTools options UNKNOWN
        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + valid video params + all EncTools options ON
        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options OFF
        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //VBR + valid video params + all EncTools options UNKNOWN
        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CQP + pairwise EncTools + valid video params
        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*15*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*19*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*20*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*21*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*22*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*23*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*24*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CBR + pairwise EncTools + valid video params
        {/*25*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*26*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*27*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*28*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*29*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*30*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*31*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*32*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*33*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*24*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*35*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*36*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*37*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*38*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*39*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*40*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + pairwise EncTools + valid video params
        {/*41*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*42*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*43*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*44*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*45*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*46*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*47*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*48*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*49*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*50*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*51*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*52*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*53*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        {/*54*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        {/*55*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        {/*56*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //Another supported GopRefDist
        //CQP + valid video params + all EncTools options ON
        {/*57*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CQP + valid video params + all EncTools options OFF
        {/*58*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CQP + valid video params + all EncTools options UNKNOWN
        {/*59*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + valid video params + all EncTools options ON
        {/*60*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CBR + valid video params + all EncTools options OFF
        {/*61*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CBR + valid video params + all EncTools options UNKNOWN
        {/*62*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + valid video params + all EncTools options ON
        {/*63*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options OFF
        {/*64*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //VBR + valid video params + all EncTools options UNKNOWN
        {/*65*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CQP + valid video params + all EncTools options ON
        {/*66*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CQP + valid video params + all EncTools options OFF
        {/*67*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CQP + valid video params + all EncTools options UNKNOWN
        {/*68*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + valid video params + all EncTools options ON
        {/*69*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CBR + valid video params + all EncTools options OFF
        {/*70*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CBR + valid video params + all EncTools options UNKNOWN
        {/*71*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + valid video params + all EncTools options ON
        {/*72*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options OFF
        {/*73*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //VBR + valid video params + all EncTools options UNKNOWN
        {/*74*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CQP + valid video params + all EncTools options ON
        {/*75*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CQP + valid video params + all EncTools options OFF
        {/*76*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CQP + valid video params + all EncTools options UNKNOWN
        {/*77*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_OFF },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 20 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 21 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 22 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //CBR + valid video params + all EncTools options ON
        {/*78*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //CBR + valid video params + all EncTools options OFF
        {/*79*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //CBR + valid video params + all EncTools options UNKNOWN
        {/*80*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

        //VBR + valid video params + all EncTools options ON
        {/*81*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_ON },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_ON } } },

        //VBR + valid video params + all EncTools options OFF
        {/*82*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_OFF },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_OFF } } },

        //VBR + valid video params + all EncTools options UNKNOWN
        {/*83*/ MFX_ERR_NONE, MFX_ERR_NONE, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.BRC, MFX_CODINGOPTION_ON },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       7000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          9000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveI, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveRefB, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptiveLTR, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantP, MFX_CODINGOPTION_UNKNOWN },
        { ETC_PAR, &tsStruct::mfxExtEncToolsConfig.AdaptivePyramidQuantB, MFX_CODINGOPTION_UNKNOWN } } },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        mfxStatus sts = MFX_ERR_NONE;
        mfxExtEncToolsConfig *enc_tools_config = nullptr;

        const char* stream = g_tsStreamPool.Get("YUV/720x480i29_jetcar_CC60f.nv12");

        //HW implementation only
        encoder.m_impl = MFX_IMPL_HARDWARE;
        encoder.MFXInit();


        if (!(encoder.m_impl & MFX_IMPL_HARDWARE)) {
            g_tsLog << "\n\nWARNING: EncTools is only supported by HW library\n\n\n";
            throw tsSKIP;
        }

        //Query stage

        //Set MFX params
        SETPARS(&encoder.m_par, MFX_PAR);

        //Set basic parameters and attach mfxExtEncToolsConfig buffer to video parameters
        encoder.initParams();

        enc_tools_config = reinterpret_cast<mfxExtEncToolsConfig*>(encoder.m_par.GetExtBuffer(MFX_EXTBUFF_ENCTOOLS_CONFIG));
        SETPARS(enc_tools_config, ETC_PAR);

        //End of ExtBRC block filling

        encoder.allocatorInit();
        TS_CHECK_MFX;

#if defined(_WIN32) || defined(_WIN64)
        encoder.setHandle();
#endif

        g_tsStatus.expect(tc.sts_query);
        encoder.Query();

        //Init stage

        g_tsStatus.expect(tc.sts_init);
        encoder.Init();

        //Encode stage
        tsRawReader reader(stream, encoder.m_pPar->mfx.FrameInfo);
        g_tsStreamPool.Reg();
        encoder.m_filler = &reader;
        encoder.AllocSurfaces();
        encoder.AllocBitstream((encoder.m_par.mfx.FrameInfo.Width * encoder.m_par.mfx.FrameInfo.Height) * 3 *frames_num);

        encoder.EncodeFrames(frames_num);
        //DUMP_BS(encoder.m_bitstream);

        //Check per frame PSNR
        tsVideoDecoder dec(MFX_CODEC_AVC);

        mfxBitstream bs = {};
        bs.Data = encoder.m_bitstream.Data;
        bs.DataLength = encoder.m_bitstream.DataLength;
        bs.MaxLength = encoder.m_bitstream.MaxLength;
        tsBitstreamReader bs_reader(bs, encoder.m_bitstream.MaxLength);
        dec.m_bs_processor = &bs_reader;

        std::unique_ptr<mfxU8[]> Y(new mfxU8[(encoder.m_par.mfx.FrameInfo.Width * encoder.m_par.mfx.FrameInfo.Height * 12) / 8]());
        mfxFrameSurface1 ref = {};
        // NV12 format is a 12 bits per pixel format
        ref.Info = encoder.m_par.mfx.FrameInfo;
        ref.Data.Y = Y.get();
        ref.Data.U = ref.Data.Y + encoder.m_par.mfx.FrameInfo.Width * encoder.m_par.mfx.FrameInfo.Height;
        ref.Data.V = ref.Data.U + 1;
        ref.Data.Pitch = MSDK_ALIGN16(encoder.m_par.mfx.FrameInfo.Width);
        Verifier v(ref, frames_num, stream);
        dec.m_surf_processor = &v;

        dec.Init();
        dec.AllocSurfaces();
        dec.DecodeFrames(frames_num);
        dec.Close();

        encoder.Close();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_et_mds_encode);
}
