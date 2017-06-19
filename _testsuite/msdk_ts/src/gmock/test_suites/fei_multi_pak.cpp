/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

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
#include <vector>
#include "random"
#include "ts_encpak.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"

#define QP_DO_NOT_CHECK 255
const mfxF64 PSNR_THRESHOLD = 30.0;

using namespace BS_AVC2;

namespace fei_multi_pak
{
    class MultiPAK
        : public tsVideoENCPAK
    {
    public:
        //variables
        unsigned int m_numPAK;
        bool m_changeMBQP;
        bool m_changeMBMV;
        std::mt19937 m_gen;
        std::vector <Ipp32u> m_refCRCVect;
        //functions
        MultiPAK(mfxFeiFunction funcEnc, mfxFeiFunction funcPak, mfxU32 CodecId = 0, bool useDefaults = true)
            : tsVideoENCPAK(funcEnc, funcPak, CodecId, useDefaults)
            , m_numPAK(1)
            , m_changeMBQP(false)
            , m_changeMBMV(false)
        {
            memset(&m_refTmp, 0, sizeof(mfxFrameSurface1));
            memset(&m_srcTmp, 0, sizeof(mfxFrameSurface1));
        };

        mfxStatus ProcessFrameAsync();
        mfxStatus ResetParser(bool freeOnly = true);
        void AllocTmpSurf(const mfxFrameInfo& fi);
    private:
        //variables
        std::vector<tsParserAVC2 *> m_parserArray;
        mfxFrameSurface1 m_refTmp;
        mfxFrameSurface1 m_srcTmp;
        std::unique_ptr <mfxU8[]>m_pRefTmp;
        std::unique_ptr <mfxU8[]>m_pSrcTmp;
        //functions
        void ChangeMBQP(mfxExtFeiPakMBCtrl & pakObject, unsigned int seed);
        void ChangeMBMV(mfxExtFeiEncMV & mv);
        void CheckingMBQP(mfxExtFeiPakMBCtrl & pakObject, unsigned int countPAK);
        void CheckingMBMV();
        void ChekingPSNR(mfxFrameSurface1& ref, mfxFrameSurface1& src, mfxU16 pictype);
        mfxU8 GetQP(MB & mb);
        mfxU32 ClipQP(mfxU32 x);
        void FieldCopy(const mfxFrameSurface1& s, mfxFrameSurface1& sTemp, mfxU16 pictype);
    };

    mfxStatus MultiPAK::ResetParser(bool freeOnly)
    {
        std::vector<tsParserAVC2 *>::iterator curr, next = m_parserArray.begin();

        if (m_numPAK < 1)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        while (next != m_parserArray.end())
        {
            curr = next++;
            delete *curr;
            m_parserArray.erase(curr);
        }

        if (freeOnly)
            return MFX_ERR_NONE;

        m_parserArray.resize(m_numPAK - 1);
        for (unsigned int i = 0; i < (m_numPAK - 1); ++i)
            m_parserArray[i] = new tsParserAVC2(INIT_MODE_PARSE_SD);

        return MFX_ERR_NONE;
    };

    void MultiPAK::ChangeMBQP(mfxExtFeiPakMBCtrl & pakObject, unsigned int seed)
    {
        m_gen.seed(seed > 0 ? seed : 1); //seed = 0 leads to error

        int deltaQP = 10;

        std::uniform_int_distribution<int> distr(-deltaQP, deltaQP);

        mfxU32 nummb = pakObject.NumMBAlloc;
        for (mfxU32 idxMB = 0; idxMB < nummb; idxMB++)
            pakObject.MB[idxMB].QpPrimeY = ClipQP(pakObject.MB[idxMB].QpPrimeY + distr(m_gen));
    };

    void MultiPAK::ChangeMBMV(mfxExtFeiEncMV & mv)
    {
        mfxU32 nummb = mv.NumMBAlloc;
        for (mfxU32 idxMB = 0; idxMB < nummb; idxMB++) {
            for (mfxU8 idxList = 0; idxList < 2; idxList++) {
                for (mfxU8 idxMV = 0; idxMV < 16; idxMV++) {
                    auto & p = mv.MB[idxMB].MV[idxMV][idxList];
                    p.x = (mfxI16)(p.x * 0.9);
                    p.y = (mfxI16)(p.y * 0.9);
                }
            }
        }
    };

    void MultiPAK::CheckingMBQP(mfxExtFeiPakMBCtrl & pakObject, unsigned int countPAK)
    {
        tsParserAVC2 *parser = m_parserArray[countPAK];
        parser->SetBuffer0(m_bitstream);

        AU& hdr = parser->ParseOrDie();

        for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
        {
            auto& nalu = *hdr.nalu[iNALU];

            if (nalu.nal_unit_type != SLICE_IDR
                && nalu.nal_unit_type != SLICE_NONIDR)
                continue;

            auto mb = nalu.slice->mb;

            mfxI32 nummb = pakObject.NumMBAlloc;
            for (mfxI32 idxMB = 0; idxMB < nummb; idxMB++) {
                mfxU8 qp = GetQP(*mb);
                if (qp != QP_DO_NOT_CHECK) {
                    if (pakObject.MB[idxMB].QpPrimeY != qp)
                    {
                        g_tsLog << "ERROR: QP after repacking is not equal to requested one\n";
                        g_tsStatus.check(MFX_ERR_ABORTED);
                    }
                }
                mb = mb->next;
            }
        }
    };

    void MultiPAK::CheckingMBMV()
    {
        Ipp32u crc = 0;
        IppStatus sts = ippsCRC32_8u(m_bitstream.Data, m_bitstream.DataLength, &crc);
        if (sts != ippStsNoErr)
        {
            g_tsLog << "ERROR: cannot calculate CRC32 IppStatus=" << sts << "\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }

        for (size_t idxCRC = 1; idxCRC < m_refCRCVect.size(); idxCRC++) {//w/o I frame/field
            if (crc == m_refCRCVect.at(idxCRC))
            {
                g_tsLog << "ERROR: MVs in the repacking don't change bit-stream\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }

    };

    void MultiPAK::ChekingPSNR(mfxFrameSurface1& ref, mfxFrameSurface1& src, mfxU16 pictype)
    {
        bool useAllocatorRef = m_pFrameAllocator && !ref.Data.Y;
        bool useAllocatorSrc = m_pFrameAllocator && !src.Data.Y;

        mfxFrameSurface1* pRef = NULL;
        mfxFrameSurface1* pSrc = NULL;

        if (useAllocatorRef)
        {
            TRACE_FUNC3(mfxFrameAllocator::Lock, m_pFrameAllocator->pthis, ref.Data.MemId, &(ref.Data));
            g_tsStatus.check(m_pFrameAllocator->Lock(m_pFrameAllocator->pthis, ref.Data.MemId, &(ref.Data)));
        }

        if (useAllocatorSrc)
        {
            TRACE_FUNC3(mfxFrameAllocator::Lock, m_pFrameAllocator->pthis, src.Data.MemId, &(src.Data));
            g_tsStatus.check(m_pFrameAllocator->Lock(m_pFrameAllocator->pthis, src.Data.MemId, &(src.Data)));
        }

        if (pictype == MFX_PICTYPE_FRAME) {
            pRef = &ref;
            pSrc = &src;
        }
        else {
            FieldCopy(ref, m_refTmp, pictype);
            FieldCopy(src, m_srcTmp, pictype);
            pRef = &m_refTmp;
            pSrc = &m_srcTmp;
        }

        tsFrame ref_frame(*pRef);
        tsFrame s_frame(*pSrc);

        mfxF64 psnr_y = PSNR(ref_frame, s_frame, 0);
        mfxF64 psnr_u = PSNR(ref_frame, s_frame, 1);
        mfxF64 psnr_v = PSNR(ref_frame, s_frame, 2);
        mfxF64 psnr_overall = (4 * psnr_y + psnr_u + psnr_v) / 6;

        if (psnr_overall < PSNR_THRESHOLD)
        {
            g_tsLog << "ERROR: Low PSNR\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }

        if (useAllocatorRef)
        {
            TRACE_FUNC3(mfxFrameAllocator::Unlock, m_pFrameAllocator->pthis, ref.Data.MemId, &(ref.Data));
            g_tsStatus.check(m_pFrameAllocator->Unlock(m_pFrameAllocator->pthis, ref.Data.MemId, &(ref.Data)));
        }

        if (useAllocatorSrc)
        {
            TRACE_FUNC3(mfxFrameAllocator::Unlock, m_pFrameAllocator->pthis, src.Data.MemId, &(src.Data));
            g_tsStatus.check(m_pFrameAllocator->Unlock(m_pFrameAllocator->pthis, src.Data.MemId, &(src.Data)));
        }
    };

    mfxU8 MultiPAK::GetQP(MB & mb)
    {
        if ((mb.MbType >= I_16x16_0_0_1 && mb.MbType <= I_16x16_3_2_1) || (mb.coded_block_pattern % 16 != 0))
            return mb.QPy;

        return QP_DO_NOT_CHECK;
    };

    mfxU32 MultiPAK::ClipQP(mfxU32 x) {
        if (x < 0)
            return 0;
        else if (x > 51)
            return 51;
        else
            return x;
    };

    void MultiPAK::FieldCopy(const mfxFrameSurface1& s, mfxFrameSurface1& sTemp, mfxU16 pictype)
    {
        mfxU32 pitch = mfxU32(s.Data.PitchHigh << 16) + s.Data.PitchLow;
        mfxU32 pitchTmp = mfxU32(sTemp.Data.PitchHigh << 16) + sTemp.Data.PitchLow;

        int inOff = 0;
        int outOff = 0;
        int outStep = 0;
        int inStep = 0;

        int inR = 0, outR = 0;
        mfxU8 *pInR, *pOutR;
        mfxU8 *pInY = (mfxU8 *)s.Data.Y;
        mfxU8 *pOutY = (mfxU8 *)sTemp.Data.Y;
        mfxU8 *pInUV = (mfxU8 *)s.Data.Y + pitch * s.Info.Height;
        mfxU8 *pOutUV = (mfxU8 *)sTemp.Data.Y + pitchTmp * sTemp.Info.Height;

        if (pictype == MFX_PICTYPE_TOPFIELD) {
            inOff = 0; inStep = 2; outOff = 0; outStep = 1;
        }
        else {
            inOff = 1; inStep = 2; outOff = 0; outStep = 1;
        }

        for (inR = inOff, outR = outOff; outR < sTemp.Info.Height; inR += inStep, outR += outStep) {
            pInR = pInY + inR * pitch;
            pOutR = pOutY + outR * pitchTmp;
            memcpy(pOutR, pInR, pitchTmp);
        }

        for (inR = inOff, outR = outOff; outR < sTemp.Info.Height / 2; inR += inStep, outR += outStep) {
            pInR = pInUV + inR * pitch;
            pOutR = pOutUV + outR * pitchTmp;
            memcpy(pOutR, pInR, pitchTmp);
        }
    };

    void MultiPAK::AllocTmpSurf(const mfxFrameInfo& fi)
    {
        if (fi.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
            return;

        mfxU16 height = fi.Height / 2;//We work with interlaced content
        mfxU16 width = fi.Width;
        //Prepare frame information
        m_refTmp.Info.FourCC = m_srcTmp.Info.FourCC = MFX_FOURCC_NV12;
        m_refTmp.Info.ChromaFormat = m_srcTmp.Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_refTmp.Data.PitchHigh = m_srcTmp.Data.PitchHigh = 0;
        m_refTmp.Data.PitchLow = m_srcTmp.Data.PitchLow = width;
        m_refTmp.Info.CropW = m_srcTmp.Info.CropW = width;
        m_refTmp.Info.Height = m_srcTmp.Info.Height = height;
        m_refTmp.Info.CropH = m_srcTmp.Info.CropH = height;
        //
        
        m_refTmp.Data.Y = new mfxU8[(width * height * 3) / 2];
        m_pRefTmp.reset(m_refTmp.Data.Y);
        m_refTmp.Data.UV = m_refTmp.Data.Y + width * height;

        m_srcTmp.Data.Y = new mfxU8[(width * height * 3) / 2];
        m_pSrcTmp.reset(m_srcTmp.Data.Y);
        m_srcTmp.Data.UV = m_srcTmp.Data.Y + width * height;
    };

    mfxStatus MultiPAK::ProcessFrameAsync()
    {
        m_ENCInput->ExtParam = enc.inbuf.data();
        m_ENCInput->NumExtParam = (mfxU16)enc.inbuf.size();
        m_ENCOutput->ExtParam = enc.outbuf.data();
        m_ENCOutput->NumExtParam = (mfxU16)enc.outbuf.size();

        mfxStatus mfxRes = tsVideoENCPAK::ProcessFrameAsync(m_session, m_ENCInput, m_ENCOutput, m_pSyncPoint);
        if (mfxRes != MFX_ERR_NONE)
            return mfxRes;

        mfxRes = tsVideoENCPAK::SyncOperation(*m_pSyncPoint, 0);
        if (mfxRes != MFX_ERR_NONE)
            return mfxRes;

        //End of the ENC

        //Picture type
        mfxU16 pictype = ((mfxExtFeiPPS*)GetExtFeiBuffer(pak.inbuf, MFX_EXTBUFF_FEI_PPS, 0))->PictureType;
        //

        mfxExtFeiPakMBCtrl & mb = *(mfxExtFeiPakMBCtrl*)GetExtFeiBuffer(pak.inbuf, MFX_EXTBUFF_FEI_PAK_CTRL, 0);//Single-field mode

        //Save reference pak object
        mfxExtFeiPakMBCtrl mbRef;
        memset(&mbRef, 0, sizeof(mfxExtFeiPakMBCtrl));
        mbRef.MB = new mfxFeiPakMBCtrl[mb.NumMBAlloc];
        mbRef.NumMBAlloc = mb.NumMBAlloc;
        memcpy(mbRef.MB, mb.MB, sizeof(mfxFeiPakMBCtrl) * mb.NumMBAlloc);

        mfxExtFeiEncMV & mv = *(mfxExtFeiEncMV*)GetExtFeiBuffer(pak.inbuf, MFX_EXTBUFF_FEI_ENC_MV, 0);//Single-field mode

        //Save reference mv
        mfxExtFeiEncMV mvRef;
        memset(&mvRef, 0, sizeof(mfxExtFeiEncMV));
        mvRef.MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[mv.NumMBAlloc];
        mvRef.NumMBAlloc = mv.NumMBAlloc;
        memcpy(mvRef.MB, mv.MB, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB) * mv.NumMBAlloc);

        for (unsigned int countPAK = 0; countPAK < m_numPAK; countPAK++) {

            bool isLastPAK = (countPAK == (m_numPAK - 1));

            //Changing for parameters
            if (!isLastPAK) {
                if (m_changeMBQP)
                    ChangeMBQP(mb, countPAK);
                else if (m_changeMBMV)
                    ChangeMBMV(mv);
            }
            else {
                if (m_changeMBQP)
                    memcpy(mb.MB, mbRef.MB, sizeof(mfxFeiPakMBCtrl) * mbRef.NumMBAlloc);
                else if (m_changeMBMV)
                    memcpy(mv.MB, mvRef.MB, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB) * mvRef.NumMBAlloc);
            }
            m_PAKInput->ExtParam = pak.inbuf.data();
            m_PAKInput->NumExtParam = (mfxU16)pak.inbuf.size();
            m_PAKOutput->ExtParam = pak.outbuf.data();
            m_PAKOutput->NumExtParam = (mfxU16)pak.outbuf.size();

            mfxRes = tsVideoENCPAK::ProcessFrameAsync(m_session, m_PAKInput, m_PAKOutput, m_pSyncPoint);
            if (mfxRes != MFX_ERR_NONE)
            {
                SAFE_DELETE_ARRAY(mbRef.MB);
                SAFE_DELETE_ARRAY(mvRef.MB);
                return mfxRes;
            }

            mfxRes = tsVideoENCPAK::SyncOperation(m_session, *m_pSyncPoint, MFX_INFINITE);
            if (mfxRes != MFX_ERR_NONE)
            {
                SAFE_DELETE_ARRAY(mbRef.MB);
                SAFE_DELETE_ARRAY(mvRef.MB);
                return mfxRes;
            }

            //PSNR cheking
            ChekingPSNR(*m_PAKInput->InSurface, *m_PAKOutput->OutSurface, pictype);

            //Checking for MB parameters
            if (!isLastPAK) {
                if (m_changeMBQP)
                    CheckingMBQP(mb, countPAK);
                else if (m_changeMBMV)
                    CheckingMBMV();
            }
            else {
                if (m_default && m_bs_processor && g_tsStatus.get() == MFX_ERR_NONE)
                {
                    g_tsStatus.check(m_bs_processor->ProcessBitstream(m_bitstream, 1));
                    TS_CHECK_MFX;
                }
            }

            m_bitstream.DataLength = 0;
        }

        SAFE_DELETE_ARRAY(mbRef.MB);
        SAFE_DELETE_ARRAY(mvRef.MB);
        return MFX_ERR_NONE;
    };

    class TestSuite : public tsSurfaceProcessor//, public tsBitstreamWriter
    {
    public:
        //variables
        tsVideoENCPAK encpak;
        MultiPAK multipak;
        static const unsigned int n_cases;

        //functions
        TestSuite()
            : //tsBitstreamWriter("bs.out")
            encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true)
            , multipak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true)
        {
            encpak.enc.m_par.mfx.FrameInfo.Width = encpak.enc.m_par.mfx.FrameInfo.CropW = 1920;
            encpak.enc.m_par.mfx.FrameInfo.Height = encpak.enc.m_par.mfx.FrameInfo.CropH = 1088;
            encpak.pak.m_par.mfx.FrameInfo.Width = encpak.pak.m_par.mfx.FrameInfo.CropW = 1920;
            encpak.pak.m_par.mfx.FrameInfo.Height = encpak.pak.m_par.mfx.FrameInfo.CropH = 1088;

            multipak.enc.m_par.mfx.FrameInfo.Width = multipak.enc.m_par.mfx.FrameInfo.CropW = 1920;
            multipak.enc.m_par.mfx.FrameInfo.Height = multipak.enc.m_par.mfx.FrameInfo.CropH = 1088;
            multipak.pak.m_par.mfx.FrameInfo.Width = multipak.pak.m_par.mfx.FrameInfo.CropW = 1920;
            multipak.pak.m_par.mfx.FrameInfo.Height = multipak.pak.m_par.mfx.FrameInfo.CropH = 1088;

            //initialization of the raw reader
            m_rawReaderFrameInfo = encpak.enc.m_par.mfx.FrameInfo;
            m_rawReaderFrameInfo.FourCC = MFX_FOURCC_YV12;

            encpak.m_filler = this;
            //encpak.m_bs_processor = this;

            multipak.m_filler = this;
            //multipak.m_bs_processor = this;
        }
        ~TestSuite() {
            if (m_reader != NULL)
                delete m_reader;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        int RunTest(unsigned int id);

        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 picStruct;
            int numPAK;
            bool changeMBQP;
            bool changeMBMV;
        };

    private:
        //variables
        mfxFrameInfo m_rawReaderFrameInfo;
        tsRawReader *m_reader;
        static const tc_struct test_case[];

        //functions
        void SetEncodeParams(tsVideoENCPAK *obj, tc_struct tc);
    };

    void TestSuite::SetEncodeParams(tsVideoENCPAK *obj, tc_struct tc)
    {
        obj->enc.m_par.mfx.FrameInfo.PicStruct = tc.picStruct;

        obj->enc.m_pPar->mfx.GopPicSize = 30;
        obj->enc.m_pPar->mfx.GopRefDist = 1;
        obj->enc.m_pPar->mfx.NumRefFrame = 2;
        obj->enc.m_pPar->mfx.TargetUsage = 4;

        obj->enc.m_pPar->AsyncDepth = 1;
        obj->enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        obj->pak.m_par.mfx = obj->enc.m_par.mfx;
        obj->pak.m_par.AsyncDepth = obj->enc.m_par.AsyncDepth;
        obj->pak.m_par.IOPattern = obj->enc.m_par.IOPattern;
    };

    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);
        return sts;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        //w/o modification
        /*00*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 3, false, false},
        /*01*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF,   3, false, false},
        /*02*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF,   3, false, false},
        //MB QP
        /*03*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 3, true, false},
        /*04*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF,   3, true, false},
        /*05*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF,   3, true, false},
        //MB MV
        /*06*/{ MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 3, false, true},
        /*07*/{ MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF,   3, false, true},
        /*08*/{ MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF,   3, false, true}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

#define VERIFY_FIELD(var, expected, name) { \
    g_tsLog << name << " = " << var << " (expected = " << expected << ")\n"; \
    if ( var != expected) { \
        g_tsLog << "ERROR: " << name <<  " in stream != expected\n"; \
        err++; \
    } \
}

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];

        const mfxU16 n_frames = 10;
        mfxStatus sts;

        SetEncodeParams(&encpak, tc);
        SetEncodeParams(&multipak, tc);

        //set picture structure for raw reader
        m_rawReaderFrameInfo.PicStruct = encpak.enc.m_par.mfx.FrameInfo.PicStruct;

        std::vector <Ipp32u> refCRCVect;
        std::vector <Ipp32u> testCRCVect;

        //Set parameters for modification of the PAK
        multipak.m_numPAK = tc.numPAK;
        multipak.m_changeMBQP = tc.changeMBQP;
        multipak.m_changeMBMV = tc.changeMBMV;
        if (multipak.m_changeMBQP)
            multipak.ResetParser(false);

        mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

        encpak.PrepareInitBuffers();
        multipak.PrepareInitBuffers();

        g_tsStatus.expect(tc.sts);

        //ref
        sts = encpak.Init();

        if (sts == MFX_ERR_NONE)
            sts = encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width * encpak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024 * 1024);

        //test
        sts = multipak.Init();

        if (sts == MFX_ERR_NONE)
            sts = multipak.AllocBitstream((multipak.enc.m_par.mfx.FrameInfo.Width * multipak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024 * 1024);

        mfxU32 count;
        mfxU32 async = TS_MAX(1, encpak.enc.m_par.AsyncDepth);
        async = TS_MIN(n_frames, async - 1);

        //ref
        m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv"), m_rawReaderFrameInfo);
        g_tsStreamPool.Reg();

        tsBitstreamCRC32 bs_crc;
        encpak.m_bs_processor = &bs_crc;

        for (count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
            for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
                encpak.PrepareFrameBuffers(field);

                sts = encpak.EncodeFrame(field);

                if (sts != MFX_ERR_NONE)
                    break;

                refCRCVect.push_back(bs_crc.GetCRC());
                multipak.m_refCRCVect.push_back(bs_crc.GetCRC());
            }

        }

        encpak.Close();
        delete m_reader;
        m_reader = NULL;

        g_tsStatus.check(sts);

        //test
        m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv"), m_rawReaderFrameInfo);
        g_tsStreamPool.Reg();

        //Allocate internal surf
        multipak.AllocTmpSurf(m_rawReaderFrameInfo);

        tsBitstreamCRC32 bs_crc_cmp;
        multipak.m_bs_processor = &bs_crc_cmp;

        for (count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
            for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
                multipak.PrepareFrameBuffers(field);

                sts = multipak.EncodeFrame(field);

                if (sts != MFX_ERR_NONE)
                    break;

                testCRCVect.push_back(bs_crc_cmp.GetCRC());
            }

        }

        if (multipak.m_changeMBQP)
            multipak.ResetParser(true);
        multipak.Close();
        delete m_reader;
        m_reader = NULL;

        g_tsLog << count << " FRAMES Encoded\n";

        g_tsStatus.check(sts);

        if (refCRCVect.size() != testCRCVect.size())
        {
            g_tsLog << "ERROR: number of encoded frames should be the same\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }

        for (unsigned int countCRC = 0; countCRC < refCRCVect.size(); countCRC++) {
            g_tsLog << "crc = " << refCRCVect.at(countCRC) << "\n";
            g_tsLog << "crc_cmp = " << testCRCVect.at(countCRC) << "\n";
            if (refCRCVect.at(countCRC) != testCRCVect.at(countCRC))
            {
                g_tsLog << "ERROR: the 2 encoded bitstreams should be the same\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_multi_pak);
};
