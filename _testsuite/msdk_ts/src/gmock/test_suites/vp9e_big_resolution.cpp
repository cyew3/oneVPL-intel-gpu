/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

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

#define MAX_VIDEO_SURFACE_DIMENSION_SIZE (16384)

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
        CHECK_4Kx4K = 0x01,
        CHECK_8Kx8K = 0x02,
        CHECK_16Kx256 = 0x04,
        CHECK_16Kx4K = 0x10,
        CHECK_16Kx16K = 0x20,
        DISABLE_DECODER = 0x0100
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

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
        {}

        ~TestSuite()
        {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, CHECK_4Kx4K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
            }
        },

        {/*01*/ MFX_ERR_NONE, CHECK_4Kx4K | DISABLE_DECODER,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
            }
        },

        {/*02*/ MFX_ERR_NONE, CHECK_8Kx8K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_8K_SIZE },
            }
        },

        {/*03*/ MFX_ERR_NONE, CHECK_8Kx8K | DISABLE_DECODER,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_8K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_8K_SIZE },
            }
        },

        {/*04 the only working connfiguration with 16K on pre-Si*/ MFX_ERR_NONE, CHECK_16Kx256,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 256 },
            }
        },

        {/*05 should work with typical encoding scenario*/ MFX_ERR_NONE, CHECK_16Kx4K | DISABLE_DECODER,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
            }
        },

        {/*06 should work with typical encoding scenario*/ MFX_ERR_NONE, CHECK_16Kx4K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_4K_SIZE },
            }
        },

        {/*07 for 16Kx16K only I-frames (still pictures) currently supported*/ MFX_ERR_NONE, CHECK_16Kx16K,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, VP9E_16K_SIZE },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 }
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        mfxVideoParam m_EncoderPar;
    public:
        BitstreamChecker(const mfxVideoParam& pa, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces, bool do_decoding = true);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 m_AllFramesSize;
        mfxU32 m_DecodedFramesCount;
        bool m_DecoderInited;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        bool m_DoDecoding;
    };

    BitstreamChecker::BitstreamChecker(const mfxVideoParam& par, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces, bool do_decoding)
        : tsVideoDecoder(MFX_CODEC_VP9)
        , m_EncoderPar(par)
        , m_AllFramesSize(0)
        , m_DecodedFramesCount(0)
        , m_DecoderInited(false)
        , m_pInputSurfaces(pSurfaces)
        , m_DoDecoding(do_decoding)
    { }

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        nFrames;

        SetBuffer(bs);

        // parse uncompressed header and check values
        tsParserVP9::UnitType& hdr = ParseOrDie();
        /*
        // Dump stream to file
        const int encoded_size = bs.DataLength;
        std::string fname = "encoded_" + std::to_string(hdr.uh.width) + "x" + std::to_string(hdr.uh.height) + "_" + std::to_string(hdr.uh.profile) + ".vp9";
        static FILE *fp_vp9 = fopen(fname.c_str(), "wb");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fflush(fp_vp9);
        */
        // do the decoder initialisation on the first encoded frame
        if (m_DoDecoding && m_DecodedFramesCount == 0 && !m_DecoderInited)
        {
            const mfxU32 headers_shift = IVF_PIC_HEADER_SIZE_BYTES + (m_DecodedFramesCount == 0 ? IVF_SEQ_HEADER_SIZE_BYTES : 0);
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

                mfxStatus alloc_status = tsSurfacePool::AllocSurfaces(m_request, !m_use_memid);
                if (alloc_status >= 0)
                {
                    m_DecoderInited = true;
                }
                else
                {
                    g_tsLog << "ERROR: Could not allocate surfaces for the decoder, status " << alloc_status;
                    throw tsFAIL;
                }
            }
            else
            {
                g_tsLog << "ERROR: Could not inilialize the decoder, Init() returned " << init_status;
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
                g_tsLog << "ERROR: DecodeFrameAsync for frame " << hdr.FrameOrder << " failed with status: " << sts;
                throw tsFAIL;
            }

            sts = SyncOperation();
            if (sts < 0)
            {
                g_tsLog << "ERROR: SyncOperation for frame " << hdr.FrameOrder << " failed with status: " << sts;
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
            msdk_atomic_dec16((volatile mfxU16*)&pInputSurface->Data.Locked);
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
            bool useAllocator = pfa && !pSurf->Data.Y;

            if (useAllocator)
            {
                TRACE_FUNC3(mfxFrameAllocator::Lock, pfa->pthis, pSurf->Data.MemId, &(pSurf->Data));
                g_tsStatus.check(pfa->Lock(pfa->pthis, pSurf->Data.MemId, &(pSurf->Data)));
            }

            mfxFrameSurface1* pStorageSurf = surfaceStorage.GetSurface();
            msdk_atomic_inc16((volatile mfxU16*)&pStorageSurf->Data.Locked);
            tsFrame in = tsFrame(*pSurf);
            tsFrame store = tsFrame(*pStorageSurf);
            store = in;
            (*m_pInputSurfaces)[m_frameOrder] = pStorageSurf;

            if (useAllocator)
            {
                TRACE_FUNC3(mfxFrameAllocator::Unlock, pfa->pthis, pSurf->Data.MemId, &(pSurf->Data));
                g_tsStatus.check(pfa->Unlock(pfa->pthis, pSurf->Data.MemId, &(pSurf->Data)));
            }
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
        std::string case_description = "TEST CASE IS: ";

        TS_START;

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
            case_description += "[NV12]";
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            if (tc.type & CHECK_4Kx4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_nv12.yuv"));
            else if (tc.type & CHECK_8Kx8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_nv12.yuv"));
            else if (tc.type & CHECK_16Kx4K || tc.type & CHECK_16Kx256)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x4096_24_nv12.yuv"));
            else if (tc.type & CHECK_16Kx16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_nv12.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            case_description += "[P010]";
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            if (tc.type & CHECK_4Kx4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_p010_shifted.yuv"));
            else if (tc.type & CHECK_8Kx8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_p010_shifted.yuv"));
            else if (tc.type & CHECK_16Kx4K || tc.type & CHECK_16Kx256)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x4096_24_p010_shifted.yuv"));
            else if (tc.type & CHECK_16Kx16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_p010_shifted.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            case_description += "[AYUV]";
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
            if (tc.type & CHECK_4Kx4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_ayuv.yuv"));
            else if (tc.type & CHECK_8Kx8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_ayuv.yuv"));
            else if (tc.type & CHECK_16Kx4K || tc.type & CHECK_16Kx256)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x4096_24_ayuv.yuv"));
            else if (tc.type & CHECK_16Kx16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_ayuv.yuv"));
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            case_description += "[Y410]";
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
            if (tc.type & CHECK_4Kx4K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_4096x4096_24_y410.yuv"));
            else if (tc.type & CHECK_8Kx8K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_7680x7680_24_y410.yuv"));
            else if (tc.type & CHECK_16Kx4K || tc.type & CHECK_16Kx256)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x4096_24_y410.yuv"));
            else if (tc.type & CHECK_16Kx16K)
                stream = const_cast<char *>(g_tsStreamPool.Get("forBehaviorTest/Kimono1_16384x16384_24_y410.yuv"));
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();

        SETPARS(m_par, MFX_PAR);

        // base class sets values for the crops, need to override them
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        // call SETPARAMS once again to assign crop values if any
        SETPARS(m_par, MFX_PAR);

        case_description += "[" + std::to_string(m_par.mfx.FrameInfo.CropW) + "x" + std::to_string(m_par.mfx.FrameInfo.CropH) + "]";
        std::string pipeline_type = tc.type & DISABLE_DECODER ? "ONLY_ENCODE" : "ENCODE+DECODE+PSNR";
        case_description += "[" + pipeline_type + "]";

#if defined WIN32 || WIN64
        if (m_par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444 && (m_par.mfx.FrameInfo.Height * 3) > MAX_VIDEO_SURFACE_DIMENSION_SIZE)
        {
            g_tsLog << ">>SKIP<<: HEIGHT > 5K not supported for 444 formats due to DXVA \n  surface dimension limitation (16384) and recon allocation demand (3 x height)\n";
            throw tsSKIP;
        }
#endif

        // IN_VIDEO_MEMORY can reduce memory consumption a little
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        BitstreamChecker bs_checker(m_par, &inputSurfaces, tc.type & DISABLE_DECODER ? false : true);
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
            bool isResolutionSupported = true;
            if (g_tsHWtype < MFX_HW_TGL && (tc.type & CHECK_16Kx16K || tc.type & CHECK_16Kx4K || tc.type & CHECK_16Kx256))
            {
                // platform < TGL - 16K stream is unsupported on current platform
                isResolutionSupported = false;
            }
            else if (g_tsHWtype < MFX_HW_ICL && tc.type & CHECK_8Kx8K)
            {
                // platform < ICL - 8K stream is unsupported on current platform
                isResolutionSupported = false;
            }

            mfxStatus query_expect_status = isResolutionSupported ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
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

            if (!isResolutionSupported)
            {
                // MFXVideoENCODE_Query() returned MFX_ERR_UNSUPPORTED for unsupported resolution
                g_tsLog << "Resolution of the stream is not supported on the current platform.\n";
                g_tsLog << "Function MFXVideoENCODE_Query returned MFX_ERR_UNSUPPORTED. It was expected. Skip this test.\n";
                throw tsSKIP;
            }
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
                tsRawReader* feeder = nullptr;
                if (tc.type & DISABLE_DECODER)
                {
                    // use a common stream reader
                    feeder = new tsRawReader(stream, m_par.mfx.FrameInfo);
                }
                else
                {
                    // use a reader with surface storing to get PSNR later
                    feeder = new SurfaceFeeder(&inputSurfaces, m_par, stream);
                }
                feeder->m_disable_shift_hack = true; // disable an additional shifting of shifted streams
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

    } catch (tsRes r)
        {
            g_tsLog << "\n$$$$$$ " << case_description << " $$$$$$\n\n";
            if (r == tsSKIP)
            {
                g_tsLog << "[  SKIPPED ] test-case was skipped\n";
                return 0;
            }

            return r;
        }

        g_tsLog << "\n$$$$$$ " << case_description << " $$$$$$\n\n";
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_big_resolution,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_big_resolution, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_big_resolution,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_big_resolution, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
