//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

#include <assert.h>
#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include "ippi.h"
#include "mfx_h264_pak.h"
#include "mfx_h264_pak_utils.h"
#include "mfx_h264_pak_pred.h"
#include "mfx_h264_pak_pack.h"
#include "mfx_h264_pak_deblocking.h"

using namespace H264Pak;

enum
{
    NUM_PACK_THREAD = 3 // number of threads when pack is run in parallel with prediction
};

class IncrementLocked
{
public:
    explicit IncrementLocked(mfxFrameData& fd) : m_fd(fd) { m_fd.Locked++; }
    ~IncrementLocked() { m_fd.Locked--; }
private:
    IncrementLocked(const IncrementLocked&);
    IncrementLocked& operator=(const IncrementLocked&);
    mfxFrameData& m_fd;
};

class MultiFrameLocker
{
public:
    MultiFrameLocker(VideoCORE& core, mfxFrameSurface& surface);
    ~MultiFrameLocker();
    mfxStatus Lock(mfxU32 label);
    mfxStatus Unlock(mfxU32 label);

private:
    MultiFrameLocker(const MultiFrameLocker&);
    MultiFrameLocker& operator=(const MultiFrameLocker&);

    enum { MAX_NUM_FRAME_DATA = 256 };
    VideoCORE& m_core;
    mfxFrameSurface& m_surface;
    mfxU8 m_locked[MAX_NUM_FRAME_DATA];
};

void PatchMbCode(const mfxFrameParamAVC& fp, const mfxSliceParamAVC& sp, mfxMbCodeAVC& mb)
{
    if (sp.FirstMbX == mb.MbXcnt && sp.FirstMbY == mb.MbYcnt)
    {
        mb.FirstMbFlag = 1;
    }

    if (sp.SliceType & MFX_SLICETYPE_P)
    {
        mb.Skip8x8Flag = 0;
        if (mb.IntraMbFlag == 0)
        {
            mb.RefPicSelect[1][0] = 0xff;
            mb.RefPicSelect[1][1] = 0xff;
            mb.RefPicSelect[1][2] = 0xff;
            mb.RefPicSelect[1][3] = 0xff;
        }
    }
    else if (mb.MbType5Bits == MBTYPE_BP_8x8 && mb.reserved3c == 1)
    {
        mb.SubMbShape = (0 == fp.Direct8x8InferenceFlag) ? 0xff : 0;

        mfxU32 predtype = mb.RefPicSelect[0][0] == 0xff
            ? SUB_MB_PRED_MODE_L1
            : mb.RefPicSelect[1][0] == 0xff
                ? SUB_MB_PRED_MODE_L0
                : SUB_MB_PRED_MODE_Bi;

        mb.SubMbPredMode = (mfxU8)((predtype << 6) | (predtype << 4) | (predtype << 2) | predtype);
    }
}

MFXVideoPAKH264::MFXVideoPAKH264(VideoCORE *core, mfxStatus *status)
: m_core(core)
{
    if (status != 0)
    {
        *status = core == 0 ? MFX_ERR_NOT_INITIALIZED : MFX_ERR_NONE;
    }
}

MFXVideoPAKH264::~MFXVideoPAKH264()
{
    Close();
}

mfxStatus MFXVideoPAKH264::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    // MFX_EXTBUFF_CODING_OPTION must present
    MFX_CHECK_NULL_PTR1(GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION));

    mfxU16 numThread = IPP_MAX(par->mfx.NumThread, 1);
    mfxU32 numMb = par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height / 256;

    m_res.m_mvdBufferSize = NUM_MV_PER_MB * numMb * sizeof(mfxI16Pair);
    m_res.m_bsBufferSize = 384 * numMb + 4096;
    m_res.m_coeffsSize = numMb * sizeof(CoeffData);
    m_res.m_mbDoneSize = numMb * sizeof(Ipp32u);

    mfxU32 resSize = m_res.m_mvdBufferSize + 16 + m_res.m_bsBufferSize + 16;

    if (numThread > 1)
    {
        resSize += m_res.m_coeffsSize + 16 + m_res.m_mbDoneSize + 16;
    }

    mfxStatus sts = m_core->AllocBuffer(resSize, MFX_MEMTYPE_PERSISTENT_MEMORY, &m_res.m_resId);
    MFX_CHECK_STS(sts);

    mfxU8* resBuf = 0;
    sts = m_core->LockBuffer(m_res.m_resId, &resBuf);
    MFX_CHECK_STS(sts);

    m_res.m_mvdBuffer = AlignValue(resBuf, 16);
    m_res.m_bsBuffer = AlignValue(m_res.m_mvdBuffer + m_res.m_mvdBufferSize, 16);

    if (numThread > 1)
    {
        m_res.m_coeffs = AlignValue(m_res.m_bsBuffer + m_res.m_bsBufferSize, 16);
        m_res.m_mbDone = AlignValue(m_res.m_coeffs + m_res.m_coeffsSize, 16);

        sts = m_threads.Create(numThread - 1);
        MFX_CHECK_STS(sts);
    }

    m_videoParam = *par;
    m_videoParam.mfx.NumThread = numThread;

    mfxU8 seqScalingList8x8[2][64];
    memset(seqScalingList8x8[0], 16, 64);
    memset(seqScalingList8x8[1], 16, 64);

    for (mfxU32 i = 0; i < 2; i++)
    {
        for (mfxU32 qpRem = 0; qpRem < 6; qpRem++)
        {
            ippiGenScaleLevel8x8_H264_8u16s_D2(
                seqScalingList8x8[i],
                8,
                m_scaling8x8.m_seqScalingMatrix8x8Inv[i][qpRem],
                m_scaling8x8.m_seqScalingMatrix8x8[i][qpRem],
                qpRem);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    if (in == 0)
    {
        //Set ones for filed that can be configured
        memset(out, 0, sizeof(*out));
        out->mfx.CodecId = 1;
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumThread = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.TargetUsage = 1;
    }
    else
    {
        //Check options for correctness
        memset(out, 0, sizeof(*out));
        out->mfx.CodecId = MFX_CODEC_AVC;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = IPP_MIN(in->mfx.TargetUsage, 7);
        out->mfx.GopPicSize = in->mfx.GopPicSize;
        out->mfx.GopRefDist = in->mfx.GopRefDist;
        out->mfx.GopOptFlag = in->mfx.GopOptFlag;
        out->mfx.IdrInterval = in->mfx.IdrInterval;
        out->mfx.RateControlMethod = IPP_MIN(in->mfx.RateControlMethod, 2);
        out->mfx.InitialDelayInKB = in->mfx.InitialDelayInKB;
        out->mfx.BufferSizeInKB = in->mfx.BufferSizeInKB;
        out->mfx.TargetKbps = IPP_MAX(in->mfx.TargetKbps, 10);
        out->mfx.MaxKbps = in->mfx.MaxKbps;

        // B frames need at least 2 ref frames
        out->mfx.NumRefFrame = IPP_MAX(in->mfx.NumRefFrame, (out->mfx.GopRefDist > 1 ? 2 : 1));
        out->mfx.EncodedOrder = 0;
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
            out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
            out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
        {
            out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }

        out->mfx.FrameInfo.Width = AlignValue(in->mfx.FrameInfo.Width, 16);
        out->mfx.FrameInfo.Width = IPP_MIN(out->mfx.FrameInfo.Width, 4096);

        mfxU32 heightAlignment = out->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE ? 16 : 32;
        out->mfx.FrameInfo.Height = AlignValue(in->mfx.FrameInfo.Height, heightAlignment);
        out->mfx.FrameInfo.Height = IPP_MIN(out->mfx.FrameInfo.Height, 4096);

        out->mfx.FrameInfo.CropX = IPP_MIN(in->mfx.FrameInfo.CropX, in->mfx.FrameInfo.Width);
        out->mfx.FrameInfo.CropY = IPP_MIN(in->mfx.FrameInfo.CropY, in->mfx.FrameInfo.Height);
        out->mfx.FrameInfo.CropW = IPP_MIN(in->mfx.FrameInfo.CropW, out->mfx.FrameInfo.Width - out->mfx.FrameInfo.CropX);
        out->mfx.FrameInfo.CropH = IPP_MIN(in->mfx.FrameInfo.CropH, out->mfx.FrameInfo.Height - out->mfx.FrameInfo.CropY);

        mfxU32 numMb = ((out->mfx.FrameInfo.Width + 15) / 16) * ((out->mfx.FrameInfo.Height + 15) / 16);
        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            numMb /= 2;
        }

        out->mfx.NumSlice = (mfxU16)IPP_MIN(in->mfx.NumSlice, numMb);

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;
        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;
        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        out->mfx.CodecProfile = in->mfx.CodecProfile;
        out->mfx.CodecLevel = in->mfx.CodecLevel;

        /* disable for now
        mfxStatus sts = CheckProfileAndLevel(*out);
        if (sts != MFX_ERR_NONE)
        {
            out->mfx.CodecProfile = 0;
            out->mfx.CodecLevel = 0;
        }
        */
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKH264::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_NULL_PTR1(request);
    request->NumFrameMin = request->NumFrameSuggested = par->mfx.GopRefDist + 2;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKH264::RunSeqHeader(mfxFrameCUC *)
{
    return MFX_ERR_NONE;
}

enum { MFX_CUC_AVC_BRC = MFX_MAKEFOURCC('B','R','C','0') };
struct mfxExtAvcBrc
{
    mfxExtBuffer Header;
    mfxF64 HrdBufferFullness;
};

inline mfxF64 GetHrdBufferFullness(const mfxFrameCUC& cuc)
{
    mfxExtAvcBrc* extBrc = (mfxExtAvcBrc *)GetExtBuffer(cuc, MFX_CUC_AVC_BRC);
    return extBrc ? extBrc->HrdBufferFullness : 0.0;
}

mfxStatus MFXVideoPAKH264::RunFramePAK(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR1(cuc->FrameParam);
    MFX_CHECK_NULL_PTR1(cuc->SliceParam);
    MFX_CHECK_NULL_PTR1(cuc->MbParam);
    MFX_CHECK_NULL_PTR1(cuc->MbParam->Mb);
    MFX_CHECK_NULL_PTR1(cuc->Bitstream);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data);
    MFX_CHECK_NULL_PTR1(cuc->ExtBuffer);

    mfxExtAvcMvData* mvs = (mfxExtAvcMvData *)
        GetExtBuffer(*cuc, MFX_CUC_AVC_MV);

    mfxExtAvcRefFrameParam* refFrameParam = (mfxExtAvcRefFrameParam *)
        GetExtBuffer(*cuc, MFX_CUC_AVC_REFFRAME);

    mfxExtAvcSeiMessage* formattedSeiMessages = (mfxExtAvcSeiMessage *)
        GetExtBuffer(*cuc, MFX_CUC_AVC_SEI_MESSAGES);

    mfxExtAvcSeiBufferingPeriod* bufferingPeriod = (mfxExtAvcSeiBufferingPeriod *)
        GetExtBuffer(*cuc, MFX_CUC_AVC_SEI_BUFFERING_PERIOD);

    mfxExtAvcSeiPicTiming* picTiming = (mfxExtAvcSeiPicTiming *)
        GetExtBuffer(*cuc, MFX_CUC_AVC_SEI_PIC_TIMING);

    MFX_CHECK_NULL_PTR1(mvs);
    MFX_CHECK_NULL_PTR1(refFrameParam);

    mfxFrameParamAVC& fp = cuc->FrameParam->AVC;
    mfxFrameSurface& fs = *cuc->FrameSurface;
    mfxBitstream bs = *cuc->Bitstream;
    bs.FrameType = fp.FrameType;
    bs.TimeStamp = fs.Data[fp.CurrFrameLabel]->TimeStamp;

    mfxU8 secondFieldFlag = fp.FieldPicFlag && fp.SecondFieldPicFlag;
    mfxU32 bottomFieldFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxExtAvcRefPicInfo& refPicInfo = refFrameParam->PicInfo[bottomFieldFlag];

    IncrementLocked incrLocked(*fs.Data[fp.CurrFrameLabel]);
    MultiFrameLocker frameLocker(*m_core, fs);
    frameLocker.Lock(fp.CurrFrameLabel);
    frameLocker.Lock(fp.RecFrameLabel);

    for (mfxU32 i = 0; i < 16; i++)
    {
        mfxU32 idx = fp.RefFrameListP[i];
        if (idx != 0xff)
        {
            frameLocker.Lock(idx);
        }
    }

    for (mfxU32 s = 0; s < m_videoParam.mfx.NumSlice; s++)
    {
        mfxSliceParamAVC& sp = cuc->SliceParam[s].AVC;
        mfxU32 firstMb = sp.FirstMbX + sp.FirstMbY * (fp.FrameWinMbMinus1 + 1);
        for (mfxU32 i = firstMb; i < firstMb + sp.NumMb; i++)
        {
            PatchMbCode(fp, sp, cuc->MbParam->Mb[i].AVC);
        }
    }

    MbPool pool(*cuc, m_res);
    bool doDeblockingWithPred = false;

    if (m_videoParam.mfx.NumThread > 1)
    {
        pool.Reset();

        ThreadPredWaveFront pred(pool, m_scaling8x8, doDeblockingWithPred);
        m_threads.Run(pred);

        if (m_videoParam.mfx.NumThread < NUM_PACK_THREAD)
        {
            pred.Do();
            m_threads.Wait();

            if (!doDeblockingWithPred && fp.RefPicFlag)
            {
                pool.Reset();
                ThreadDeblocking deblocking(pool);

                m_threads.Run(deblocking);
                deblocking.Do();
                m_threads.Wait();
            }
        }
    }

    const mfxExtCodingOption& opt = GetExtCodingOption(m_videoParam);
    m_bitstream.StartPicture(fp, m_res.m_bsBuffer, m_res.m_bsBufferSize);

    mfxStatus packStatus = MFX_ERR_NONE;
    if (GetMfxOptDefaultOn(opt.AUDelimiter))
    {
        m_bitstream.PackAccessUnitDelimiter(fp.FrameType);
        packStatus = m_bitstream.GetRbsp(bs);
        //MFX_CHECK_STS(packStatus); // Continue since need to report size of picture to BRC
    }

    if ((fp.FrameType & MFX_FRAMETYPE_I) && !secondFieldFlag)
    {
        m_bitstream.PackSeqParamSet(m_videoParam, fp, true);
        packStatus = m_bitstream.GetRbsp(bs);
    }

    m_bitstream.PackPicParamSet(m_videoParam, fp);
    packStatus = m_bitstream.GetRbsp(bs);

    m_bitstream.PackSei(bufferingPeriod, picTiming, formattedSeiMessages);
    packStatus = m_bitstream.GetRbsp(bs);

    for (mfxU32 s = 0; s < m_videoParam.mfx.NumSlice; s++)
    {
        mfxSliceParamAVC& sp = cuc->SliceParam[s].AVC;
        mfxU32 firstMb = sp.FirstMbX + sp.FirstMbY * (fp.FrameWinMbMinus1 + 1);

        m_bitstream.PackSliceHeader(m_videoParam, fp, *refFrameParam, sp);
        m_bitstream.StartSlice(fp, sp);

        if (m_videoParam.mfx.NumThread == 1)
        {
            for (mfxU32 i = firstMb; i < firstMb + sp.NumMb; i++)
            {
                CoeffData coeffData;

                MbDescriptor mb(*cuc, m_res.m_mvdBuffer, i);

                PredictMb(
                    fp,
                    refPicInfo.SliceInfo[mb.sliceIdN],
                    fs,
                    mb,
                    m_scaling8x8.m_seqScalingMatrix8x8,
                    m_scaling8x8.m_seqScalingMatrix8x8Inv,
                    coeffData);

                m_bitstream.PackMb(fp, sp, mb, coeffData);
            }
        }
        else if (m_videoParam.mfx.NumThread < NUM_PACK_THREAD)
        {
            // packMb runs when all predictMb finishes
            for (mfxU32 i = firstMb; i < firstMb + sp.NumMb; i++)
            {
                MbDescriptor mb(*cuc, m_res.m_mvdBuffer, i);
                m_bitstream.PackMb(fp, sp, mb, pool.m_coeffs[i]);
            }
        }
        else
        {
            // packMb runs in parallel with predictMb
            for (mfxU32 i = firstMb; i < firstMb + sp.NumMb; i++)
            {
                while (pool.m_mbDone[i] != MbPool::MB_DONE);

                MbDescriptor mb(*cuc, m_res.m_mvdBuffer, i);
                m_bitstream.PackMb(fp, sp, mb, pool.m_coeffs[i]);
            }
        }

        m_bitstream.EndSlice();
        packStatus = m_bitstream.GetRbsp(bs);
    }

    if (m_videoParam.mfx.NumThread >= NUM_PACK_THREAD)
    {
        m_threads.Wait();

        if (!doDeblockingWithPred && fp.RefPicFlag)
        {
            pool.Reset();
            ThreadDeblocking deblocking(pool);

            m_threads.Run(deblocking);
            deblocking.Do();
            m_threads.Wait();
        }
    }

    if (fp.RefPicFlag && m_videoParam.mfx.NumThread == 1)
    {
        for (mfxU32 i = 0; i < fp.NumMb; i++)
        {
            MbDescriptor mbToDeblock(*cuc, 0, i, true);
            mfxSliceParamAVC& slice = cuc->SliceParam[mbToDeblock.sliceIdN].AVC;
            if (slice.DeblockDisableIdc == 0)
            {
                DeblockMb(fp, slice, refPicInfo, mbToDeblock, fs);
            }
        }
    }

    *cuc->Bitstream = bs;
    return packStatus;
}

mfxStatus MFXVideoPAKH264::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    Close();
    return Init(par);
}

mfxStatus MFXVideoPAKH264::Close(void)
{
    m_threads.Destroy();

    if (m_res.m_resId != 0)
    {
        m_core->UnlockBuffer(m_res.m_resId);
        m_core->FreeBuffer(m_res.m_resId);
    }

    m_res.m_resId = 0;
    m_res.m_bsBuffer = 0;
    m_res.m_coeffs = 0;
    m_res.m_mbDone = 0;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKH264::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par)
    *par = m_videoParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPAKH264::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par)
    *par = m_frameParam;
    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODER
