/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <mutex>
#include <map>
#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_big_resolution
{
#define VP9E_4K_SIZE (4096)
#define VP9E_8K_SIZE (7680)
#define VP9E_16K_SIZE (16384)

#define PSNR_THRESHOLD (35)

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12
#define MAX_IVF_HEADER_SIZE IVF_SEQ_HEADER_SIZE_BYTES + IVF_PIC_HEADER_SIZE_BYTES

    enum
    {
        NONE,
        MFX_PAR,
    };

    enum
    {
        CHECK_4K,
        CHECK_8K,
        CHECK_16K
    };


    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        static const unsigned int n_cases;
        mfxExtVP9Segmentation *m_InitedSegmentExtParams;
        mfxU32 m_SourceWidth;
        mfxU32 m_SourceHeight;
        std::mutex  m_frames_storage_mtx;


        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
            , m_SourceWidth(0)
            , m_SourceHeight(0)
        {}

        ~TestSuite()
        {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);

        mfxVideoParam const & GetEncodingParams() { return m_par; }

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, CHECK_4K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
            }
        },

        {/*01*/ MFX_ERR_NONE, CHECK_8K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_8K_SIZE },
            }
        },

        {/*02*/ MFX_ERR_NONE, CHECK_16K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_16K_SIZE },
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        mfxVideoParam m_EncoderPar;
    public:
        BitstreamChecker(const mfxVideoParam& pa, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 m_AllFramesSize;
        mfxU32 m_DecodedFramesCount;
        bool m_DecoderInited;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
    };

    BitstreamChecker::BitstreamChecker(const mfxVideoParam& par, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces)
        : tsVideoDecoder(MFX_CODEC_VP9)
        , m_EncoderPar(par)
        , m_AllFramesSize(0)
        , m_DecodedFramesCount(0)
        , m_DecoderInited(false)
        , m_pInputSurfaces(pSurfaces)
    { }

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        SetBuffer(bs);
        /*
        // Dump stream to file
        const int encoded_size = bs.DataLength;
        static FILE *fp_vp9 = fopen("vp9e_encoded_segmentation.vp9", "wb");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fflush(fp_vp9);
        */

        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            // do the decoder initialisation on the first encoded frame
            if (m_DecodedFramesCount == 0)
            {
                const mfxU32 headers_shift = 12/*header*/ + (m_DecodedFramesCount == 0 ? 32/*ivf_header*/ : 0);
                m_pBitstream->Data = bs.Data + headers_shift;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = bs.DataLength - headers_shift;
                m_pBitstream->MaxLength = bs.MaxLength;

                m_pPar->AsyncDepth = 1;

                mfxStatus decode_header_status = DecodeHeader();

                mfxStatus init_status = Init();
                m_par_set = true;
                if (init_status >= 0)
                {
                    if (m_default && !m_request.NumFrameMin)
                    {
                        QueryIOSurf();
                    }
                    // This is a workaround for the decoder (there is an issue with NumFrameSuggested and decoding superframes)
                    m_request.NumFrameMin = 5;
                    m_request.NumFrameSuggested = 10;

                    mfxStatus alloc_status = tsSurfacePool::AllocSurfaces(m_request, !m_use_memid);
                    if (alloc_status >= 0)
                    {
                        m_DecoderInited = true;
                    }
                    else
                    {
                        ADD_FAILURE() << "WARNING: Could not allocate surfaces for the decoder, status " << alloc_status;
                        throw tsFAIL;
                    }
                }
                else
                {
                    ADD_FAILURE() << "WARNING: Could not inilialize the decoder, Init() returned " << init_status;
                    throw tsFAIL;
                }
            }

            if (m_DecoderInited)
            {
                const mfxU32 ivfSize = hdr.FrameOrder == 0 ? MAX_IVF_HEADER_SIZE : IVF_PIC_HEADER_SIZE_BYTES;

                m_pBitstream->Data = bs.Data + ivfSize;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = bs.DataLength - ivfSize;
                m_pBitstream->MaxLength = bs.MaxLength;

                mfxStatus sts = MFX_ERR_NONE;
                do
                {
                    sts = DecodeFrameAsync();
                } while (sts == MFX_WRN_DEVICE_BUSY);

                if (sts < 0)
                {
                    ADD_FAILURE() << "ERROR: DecodeFrameAsync for frame " << hdr.FrameOrder << " failed with status: " << sts;
                    throw tsFAIL;
                }

                sts = SyncOperation();
                if (sts < 0)
                {
                    ADD_FAILURE() << "ERROR: SyncOperation for frame " << hdr.FrameOrder << " failed with status: " << sts;
                    throw tsFAIL;
                }

                // check that respective source frame is stored
                if (m_pInputSurfaces->find(hdr.FrameOrder) == m_pInputSurfaces->end())
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                mfxU32 w = m_pPar->mfx.FrameInfo.Width;
                mfxU32 h = m_pPar->mfx.FrameInfo.Height;

                mfxFrameSurface1* pInputSurface = (*m_pInputSurfaces)[hdr.FrameOrder];
                tsFrame src = tsFrame(*pInputSurface);
                tsFrame res = tsFrame(*m_pSurf);
                src.m_info.CropX = src.m_info.CropY = res.m_info.CropX = res.m_info.CropY = 0;
                src.m_info.CropW = res.m_info.CropW = w;
                src.m_info.CropH = res.m_info.CropH = h;

                const mfxF64 psnrY = PSNR(src, res, 0);
                const mfxF64 psnrU = PSNR(src, res, 1);
                const mfxF64 psnrV = PSNR(src, res, 2);
                pInputSurface->Data.Locked--;
                m_pInputSurfaces->erase(hdr.FrameOrder);
                const mfxF64 minPsnr = PSNR_THRESHOLD;

                g_tsLog << "INFO: frame[" << hdr.FrameOrder << "]: PSNR-Y=" << psnrY << " PSNR-U="
                    << psnrU << " PSNR-V=" << psnrV << " size=" << bs.DataLength <<"\n";

                if (psnrY < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-Y of frame " << hdr.FrameOrder << " is equal to " << psnrY << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
                if (psnrU < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-U of frame " << hdr.FrameOrder << " is equal to " << psnrU << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
                if (psnrV < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-V of frame " << hdr.FrameOrder << " is equal to " << psnrV << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
            }

            m_AllFramesSize += bs.DataLength;
            bs.DataLength = bs.DataOffset = 0;

            m_DecodedFramesCount++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    class SurfaceFeeder : public tsRawReader
    {
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        mfxU32 m_frameOrder;

        tsSurfacePool surfaceStorage;
    public:
        SurfaceFeeder(
            std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces,
            const mfxVideoParam& par,
            const char* fname,
            mfxU32 n_frames = 0xFFFFFFFF)
            : tsRawReader(fname, par.mfx.FrameInfo, n_frames)
            , m_pInputSurfaces(pInSurfaces)
            , m_frameOrder(0)
        {
            if (m_pInputSurfaces)
            {
                const mfxU32 numInternalSurfaces = par.AsyncDepth ? par.AsyncDepth : 5;
                mfxFrameAllocRequest req = {};
                req.Info = par.mfx.FrameInfo;
                req.NumFrameMin = req.NumFrameSuggested = numInternalSurfaces;
                req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
                surfaceStorage.AllocSurfaces(req);
            }
        };

        ~SurfaceFeeder()
        {
            if (m_pInputSurfaces)
            {
                surfaceStorage.FreeSurfaces();
            }
        }

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
    };

    mfxFrameSurface1* SurfaceFeeder::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        mfxFrameSurface1* pSurf = tsSurfaceProcessor::ProcessSurface(ps, pfa);
        if (m_pInputSurfaces)
        {
            mfxFrameSurface1* pStorageSurf = surfaceStorage.GetSurface();
            pStorageSurf->Data.Locked++;
            tsFrame in = tsFrame(*pSurf);
            tsFrame store = tsFrame(*pStorageSurf);
            store = in;
            (*m_pInputSurfaces)[m_frameOrder] = pStorageSurf;
        }
        m_frameOrder++;

        return pSurf;
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        if (g_tsHWtype < MFX_HW_TGL && tc.type == CHECK_16K)
        {
            g_tsLog << "WARNING: the case unsupported and skipped for current platform!\n";
            return 0;
        }
        else if (g_tsHWtype < MFX_HW_ICL && tc.type == CHECK_8K)
        {
            g_tsLog << "WARNING: the case unsupported and skipped for current platform!\n";
            return 0;
        }

        tsRawReader *reader = nullptr;
        std::map<mfxU32, mfxFrameSurface1*> inputSurfaces;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = 50;
        m_par.AsyncDepth = 1;

        char* stream = nullptr;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            if (tc.type == CHECK_4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_nv12.yuv"));
            else if (tc.type == CHECK_8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_nv12.yuv"));
            else if (tc.type == CHECK_16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_nv12.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            if (tc.type == CHECK_4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_p010_shifted.yuv"));
            else if (tc.type == CHECK_8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_p010_shifted.yuv"));
            else if (tc.type == CHECK_16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_p010_shifted.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            if (tc.type == CHECK_4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_ayuv.yuv"));
            else if (tc.type == CHECK_8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_ayuv.yuv"));
            else if (tc.type == CHECK_16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_ayuv.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            if (tc.type == CHECK_4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_y410.yuv"));
            else if (tc.type == CHECK_8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_y410.yuv"));
            else if (tc.type == CHECK_16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_y410.yuv"));
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();

        SETPARS(m_par, MFX_PAR);

        m_SourceWidth = m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_SourceHeight = m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        BitstreamChecker bs_checker(m_par, &inputSurfaces);
        m_bs_processor = &bs_checker;

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        // QUERY SECTION
        {
            mfxStatus query_expect_status = MFX_ERR_NONE;

            g_tsStatus.expect(query_expect_status);
            tsExtBufType<mfxVideoParam> par_query_out = m_par;

            TS_TRACE(m_session);
            TS_TRACE(m_pPar);
            g_tsLog << "NOW CALLING QUERY()...\n MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out) \n";
            mfxStatus query_result_status = MFXVideoENCODE_Query(m_session, m_pPar, &par_query_out);
            g_tsLog << "QUERY() FINISHED WITH STATUS " << query_result_status << "\n";
            TS_TRACE(&par_query_out);

            g_tsLog << "Query() returned with status " << query_result_status << ", expected status " << query_expect_status << "\n";
            g_tsStatus.check(query_result_status);
        }

        // INIT SECTION
        {
            g_tsStatus.expect(tc.sts);
            Init();
        }

        // ENCODE SECTION
        {
            g_tsStatus.expect(tc.sts);

            //set reader
            if (reader == nullptr)
            {
                tsRawReader* feeder = new SurfaceFeeder(&inputSurfaces, m_par, stream);
                m_filler = feeder;
            }

            mfxU32 encoded = 0;
            mfxU32 submitted = 0;
            mfxSyncPoint sp;
            const mfxU32 frames_count = 2;

            while (encoded < frames_count)
            {
                //submit source frames to the encoder
                m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;
                if (submitted < frames_count)
                {
                    g_tsLog << "INFO: Submitting frame #" << submitted << "\n";
                    if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                    {
                        submitted++;
                        continue;
                    }
                    else
                    {
                        submitted++;
                    }
                }
                else if (m_pPar->AsyncDepth > 1)
                {
                    g_tsLog << "INFO: Draining encoded frames\n";
                    mfxStatus mfxRes_drain = EncodeFrameAsync(m_session, 0, NULL, m_pBitstream, m_pSyncPoint);
                    if (MFX_ERR_MORE_DATA == mfxRes_drain)
                    {
                        break;
                    }
                    else if (mfxRes_drain != MFX_ERR_NONE)
                    {
                        ADD_FAILURE() << "ERROR: Unexpected state on drain: " << mfxRes_drain << "\n";
                        throw tsFAIL;
                    }
                }

                g_tsStatus.check(); TS_CHECK_MFX;
                sp = m_syncpoint;

                SyncOperation(); TS_CHECK_MFX;
                encoded++;
            }
        }

        if (m_initialized)
        {
            g_tsStatus.expect(MFX_ERR_NONE);
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_big_resolution,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_big_resolution, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_big_resolution,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_big_resolution, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
