/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <vector>
#include "ts_fei_warning.h"

/* Test checks whether encoder follows a defined GOP structure with IDR requests.
 * Tests checks PSNR of resulted bistream to make sure there are no reordering issues
 */

// #define DUMP_BS

#define MAX_IDR 30

namespace hevce_fei_idr_insertion
{
    using namespace BS_HEVC2;

    const int MFX_PAR = 1;

    const mfxF64 PSNR_THRESHOLD = 45.0;

    typedef std::map<mfxU64,std::pair<mfxU32, mfxU8>> FrameMap; // <TimeStamp, <FrameOrder, FrameType>>

    mfxU8 GetFrameType(
        mfxVideoParam const & video,
        mfxU32                pictureOrder,
        bool                  isPictureOfLastFrame)
    {
        mfxU32 gopOptFlag = video.mfx.GopOptFlag;
        mfxU32 gopPicSize = video.mfx.GopPicSize;
        mfxU32 gopRefDist = video.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval);

        if (gopPicSize == 0xffff)
            idrPicDist = gopPicSize = 0xffffffff;

        bool bFields = !!(video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);

        mfxU32 frameOrder = bFields ? pictureOrder / 2 : pictureOrder;

        bool   bSecondField = bFields && (pictureOrder & 1);
        bool   bIdr = (idrPicDist ? frameOrder % idrPicDist : frameOrder) == 0;

        if (bIdr)
            return bSecondField ? MFX_FRAMETYPE_P : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR);

        if (frameOrder % gopPicSize == 0)
            return bSecondField ? MFX_FRAMETYPE_P : MFX_FRAMETYPE_I;

        if (frameOrder % gopPicSize % gopRefDist == 0)
            return MFX_FRAMETYPE_P;

        if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        {
            if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
                (idrPicDist && (frameOrder + 1) % idrPicDist == 0) ||
                isPictureOfLastFrame)
            {
                return MFX_FRAMETYPE_P;
            }
        }

        return MFX_FRAMETYPE_B;
    }

    class TestSuite
        : public tsBitstreamProcessor
        , public tsSurfaceProcessor
        , public tsParserHEVC2
        , public tsVideoEncoder
    {
    private:
        tsRawReader* m_reader;

        mfxU32 m_fo = 0;
        FrameMap m_map;
        mfxU32 m_lastIDRInDisplayOrder = 0;
        mfxU32 m_currIDRIndex = 0;

    public:
        static const unsigned int n_cases;

        typedef struct
        {
            mfxU32 nFrames;
            mfxU8 nIdrRequests;
            mfxU32 IDRs[MAX_IDR];
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        } tc_struct;

        static const tc_struct test_case[];

        const tc_struct * m_tc;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        {
            m_filler = this;
        }

        ~TestSuite(){}

        int RunTest(unsigned int id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_reader->ProcessSurface(s);

            s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

            const bool bFields = !!(m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);

            bool isPictureOfLastFrameInGOP = false;

            if ((s.Data.FrameOrder >> bFields) == ((m_max - 1) >> bFields)) // EOS
                isPictureOfLastFrameInGOP = true;

            const mfxU32* begin = &m_tc->IDRs[0];
            const mfxU32* end   = &m_tc->IDRs[m_tc->nIdrRequests];

            bool bFirstField = bFields && !(s.Data.FrameOrder & 1);
            if (bFirstField)
            {
                // Check the 2nd field in the pair
                const mfxU32 nextFieldOrder = s.Data.FrameOrder + 1;
                if (std::find(begin, end, nextFieldOrder) != end)
                    isPictureOfLastFrameInGOP = true;
            }

            // Check next Frame or the 1st field of next Frame
            const mfxU32 nextFrameOrder = (bFields ? ( bFirstField ? 2 : 1) : 1) + s.Data.FrameOrder;

            if (std::find(begin, end, nextFrameOrder) != end)
                isPictureOfLastFrameInGOP = true;

            mfxU8 type = GetFrameType(m_par, (bFields ? (m_lastIDRInDisplayOrder % 2) : 0) + s.Data.FrameOrder - m_lastIDRInDisplayOrder, isPictureOfLastFrameInGOP);

            if (m_currIDRIndex < m_tc->nIdrRequests && m_tc->IDRs[m_currIDRIndex] == s.Data.FrameOrder)
            {
                m_currIDRIndex++;
                m_pCtrl->FrameType = type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR;
            }
            else
                m_pCtrl->FrameType = 0;

            if (type & MFX_FRAMETYPE_IDR)
            {
                m_lastIDRInDisplayOrder = s.Data.FrameOrder;
            }

            m_map[s.Data.TimeStamp] = std::make_pair(s.Data.FrameOrder, type);

            return MFX_ERR_NONE;
        }
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        BitstreamChecker(TestSuite *testPtr, FrameMap & map, const mfxVideoParam & par, mfxU32 nFrames);
        ~BitstreamChecker();
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

        mfxBitstream m_bst;
    private:
        FrameMap & m_map;
#ifdef DUMP_BS
        tsBitstreamWriter m_writer;
#endif
        const mfxVideoParam& m_par;
        mfxU32 m_nFrames;

    };

    BitstreamChecker::BitstreamChecker(TestSuite *testPtr, FrameMap & map, const mfxVideoParam & par, mfxU32 nFrames)
        : m_map(map)
#ifdef DUMP_BS
        , m_writer("debug.265")
#endif
        , m_par(par)
        , m_nFrames(nFrames)
    {
        mfxU32 bs_size = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 / 2 * m_nFrames;
        m_bst.Data = new mfxU8[bs_size];
        m_bst.DataOffset = 0;
        m_bst.DataLength = 0;
        m_bst.MaxLength = bs_size;
    }
    BitstreamChecker::~BitstreamChecker()
    {
        delete[] m_bst.Data;
    }

    inline bool isIDR(const NALU& nalu){
        return ((nalu.nal_unit_type == IDR_W_RADL ) || (nalu.nal_unit_type == IDR_N_LP));
    }

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        if (m_bst.MaxLength >= m_bst.DataLength + bs.DataLength)
        {
            memcpy(&m_bst.Data[m_bst.DataLength], &bs.Data[bs.DataOffset], bs.DataLength);
            m_bst.DataLength += bs.DataLength;
        }
        else
            return MFX_ERR_ABORTED;

#ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
#endif
        const mfxU64 & FrameOrderInEncodedOrder = m_map[bs.TimeStamp].first;
        const mfxU8 expectedType = m_map[bs.TimeStamp].second;
        while (nFrames--)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                auto & sh = *pNALU->slice;

                mfxU32 L0Size = sh.num_ref_idx_l0_active;
                mfxU32 L1Size = sh.num_ref_idx_l1_active;

                auto IsInL0 = [&](const RefPic & frame)
                {
                    const RefPic* begin = &sh.L0[0];
                    const RefPic* end   = &sh.L0[L0Size];
                    const RefPic* it = std::find_if(begin, end, [&](const RefPic& L0Frame) { return L0Frame.POC == frame.POC;});
                    return it != end ? true : false;
                };
                bool isGPB = (sh.type == SLICE_TYPE::B) && std::all_of(&sh.L1[0], &sh.L1[L1Size], IsInL0) ? true : false;
                if (isGPB)
                    sh.type = SLICE_TYPE::P;

                static const mfxU8 frameTypes[] = { MFX_FRAMETYPE_B, MFX_FRAMETYPE_P, MFX_FRAMETYPE_I};
                mfxU8 actualType = isIDR(*pNALU) ? frameTypes[sh.type] | MFX_FRAMETYPE_IDR : frameTypes[sh.type];

                g_tsLog << "POC " << sh.POC << "\n";
                g_tsLog << " - FrameOrder " << FrameOrderInEncodedOrder
                        << "\n"
                        << " - Actual: " << actualType
                        << " - Expected: " << expectedType << "\n";
                EXPECT_EQ(expectedType, actualType);
            }
        }
        m_map.erase(bs.TimeStamp);

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    class PSNRVerifier : public tsSurfaceProcessor
    {
    public:
        tsRawReader* m_reader;
        mfxFrameSurface1 m_refSrf;
        mfxU32 m_fo;

        PSNRVerifier(mfxFrameSurface1& s, mfxU16 nFrames, const char* stream)
            : tsSurfaceProcessor()
            , m_reader(0)
        {
            m_reader = new tsRawReader(stream, s.Info, nFrames);
            m_max = nFrames;
            m_refSrf = s;
            m_fo = 0;
        }

        ~PSNRVerifier()
        {
            if (m_reader)
            {
                delete m_reader;
                m_reader = NULL;
            }
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_reader->ProcessSurface(m_refSrf);

            tsFrame ref_frame(m_refSrf);
            tsFrame out_frame(s);
            mfxF64 psnr_y = PSNR(ref_frame, out_frame, 0);
            mfxF64 psnr_u = PSNR(ref_frame, out_frame, 1);
            mfxF64 psnr_v = PSNR(ref_frame, out_frame, 2);
            if ((psnr_y < PSNR_THRESHOLD) ||
                (psnr_u < PSNR_THRESHOLD) ||
                (psnr_v < PSNR_THRESHOLD))
            {
                g_tsLog << "ERROR: Low PSNR(Y " << psnr_y
                        << ", U " << psnr_u
                        << ", V " << psnr_v
                        << ") on frame " << m_fo << "\n";
                return MFX_ERR_ABORTED;
            }
            m_fo++;
            return MFX_ERR_NONE;
        }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/30,  7, {1, 3, 5, 7, 10, 15, 16},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0 },
                        }
        },
        {/*01*/30,  7, {1, 3, 4, 7, 10, 15, 28},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
        {/*02*/30,  MAX_IDR, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                        }
        },
        {/*03*/60,  MAX_IDR, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
        {/*04*/30,  6, {1, 3, 5, 7, 10, 20},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                        }
        },
        {/*05*/60,  9, {1, 3, 4, 7, 10, 20, 41, 42, 43},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
        {/*06*/30,  5, {4, 10, 13, 16, 29},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                        }
        },
        {/*07*/61,  10, {8, 9, 19, 20, 26, 31, 36, 43, 48, 59},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
        {/*08*/33,  0, {},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 1 },
                        }
        },
        {/*09*/65,  0, {},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 1 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
        {/*10*/33,  5, {4, 10, 13, 16, 29},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 1 },
                        }
        },
        {/*11*/65,  9, {8, 9, 19, 20, 26, 36, 43, 48, 63},
                        {
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 1 },
                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
                        }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        const tc_struct& tc = test_case[id];

        TS_START;
        CHECK_HEVC_FEI_SUPPORT();

        m_tc = &tc;

        mfxExtCodingOption3& co3 = m_par;

        SETPARS(m_pPar, MFX_PAR);
        m_par.mfx.FrameInfo.Width  = 352;
        m_par.mfx.FrameInfo.Height = 288;
        m_par.mfx.FrameInfo.CropW  = 352;
        m_par.mfx.FrameInfo.CropH  = 288;

        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 10;

        m_max = tc.nFrames;
        m_par.AsyncDepth = 1;

        const char* stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
        g_tsStreamPool.Reg();
        m_reader = new tsRawReader(stream, m_par.mfx.FrameInfo, tc.nFrames);

        BitstreamChecker bs_checker(this, m_map, m_par, tc.nFrames);
        m_bs_processor = &bs_checker;

        Init();
        GetVideoParam();

        mfxExtFeiHevcEncFrameCtrl & ctrl = m_ctrl;
        ctrl.SubPelMode         = 3;
        ctrl.SearchWindow       = 5;
        ctrl.NumFramePartitions = 4;

        EncodeFrames(tc.nFrames);

        tsVideoDecoder dec(MFX_CODEC_HEVC);
        tsBitstreamReader reader(bs_checker.m_bst, bs_checker.m_bst.DataLength);
        dec.m_bs_processor = &reader;

        mfxFrameSurface1 *ref = GetSurface();
        PSNRVerifier v(*ref, tc.nFrames, stream);
        dec.m_surf_processor = &v;

        dec.m_pPar->AsyncDepth = 1;
        dec.Init();
        dec.AllocSurfaces();

        dec.DecodeFrames(tc.nFrames);

        dec.Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_idr_insertion);
}
