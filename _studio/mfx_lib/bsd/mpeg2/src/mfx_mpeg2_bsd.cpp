/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_bsd.cpp

\* ****************************************************************************** */
#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)

#include "mfx_mpeg2_bsd.h"
#include "mfx_mpeg2_bsd_utils.h"
#include "mfx_mpeg2_dec_common.h"

using namespace MfxMpeg2Bsd;

VideoBSDMPEG2::VideoBSDMPEG2(VideoCORE *core, mfxStatus *sts)
: VideoBSD()
, m_implUMC()
, m_core(core)
, m_bVlcTableInited(false)
{
    *sts = MFX_ERR_NONE;
}

VideoBSDMPEG2::~VideoBSDMPEG2(void)
{
    Close();
}

mfxStatus VideoBSDMPEG2::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus VideoBSDMPEG2::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus VideoBSDMPEG2::Close(void)
{
    if (m_bVlcTableInited)
    {
        m_implUMC.DeleteHuffmanTables();
        m_bVlcTableInited = false;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    if (!in)
    { // configurability mode
        Mpeg2SetConfigurableCommon(*out);
    }
    else
    { // checking mode
        memcpy(out, in, sizeof(mfxVideoParam));
        Mpeg2CheckConfigurableCommon(*out);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_vPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_sPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunVideoParam(mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);
    BufferLocker locker(*m_core, *bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    UMC::Mpeg2Bitstream mpeg2bs;
    ConvertBitStreamMfx2Umc(*bs, mpeg2bs);
    // find sequence header start code
    Ipp32u code;
    for (;;)
    {
        FIND_START_CODE(mpeg2bs.bs, code);
        if (code == (Ipp32u)UMC::UMC_ERR_NOT_ENOUGH_DATA)
            return ConvertStatusUmc2Mfx(UMC::UMC_ERR_NOT_ENOUGH_DATA);
        else if (code == SEQUENCE_HEADER_CODE)
        {
            UMC::Status umcRes = m_implUMC.UnpackSequenceHeader(mpeg2bs, m_seqHead);
            MFX_CHECK_UMC_STS(umcRes);
            m_qMatrixChanged = true;
            break;
        }
        else
        {
            SKIP_BITS_32(mpeg2bs.bs);
        }
    }

    if (!m_seqHead.seqHeadExtPresent) // MPEG1
        MFX_BSD_UNSUPPORTED("Mpeg1 is unsupported yet");
    if (m_seqHead.chroma_format != 1) // not 4:2:0
        MFX_BSD_UNSUPPORTED("Only 4:2:0 is supported");
    if (m_vPar.mfx.FrameInfo.CropW > 0 && m_vPar.mfx.FrameInfo.CropW != (mfxU16)m_seqHead.horizontal_size)
        MFX_BSD_UNSUPPORTED("Width changing is unsupported yet");
    if (m_vPar.mfx.FrameInfo.CropH > 0 && m_vPar.mfx.FrameInfo.CropH != (mfxU16)m_seqHead.vertical_size)
        MFX_BSD_UNSUPPORTED("Height changing is unsupported yet");
    if (m_vPar.mfx.FrameInfo.CropW > MFX_MPEG2_MAX_WIDTH || m_vPar.mfx.FrameInfo.CropH > MFX_MPEG2_MAX_HEIGHT)
        MFX_BSD_UNSUPPORTED("Width and height greater than 4095 isn't supported yet");

    ConvertBitStreamUmc2Mfx(mpeg2bs, *bs);
    PopulateMfxVideoParam(m_vPar);
    if (par != &m_vPar)
        *par = m_vPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunFrameParam(mfxBitstream *bs, mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);
    BufferLocker locker(*m_core, *bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    UMC::Mpeg2Bitstream mpeg2bs;
    ConvertBitStreamMfx2Umc(*bs, mpeg2bs);
    // find picture header start code
    Ipp32u code;
    for (;;)
    {
        FIND_START_CODE(mpeg2bs.bs, code);
        if (code == UMC::UMC_ERR_NOT_ENOUGH_DATA)
            return ConvertStatusUmc2Mfx(UMC::UMC_ERR_NOT_ENOUGH_DATA);

        if (code == SEQUENCE_HEADER_CODE)
        {
            ConvertBitStreamUmc2Mfx(mpeg2bs, *bs);
            mfxStatus mfxRes = RunVideoParam(bs, &m_vPar);
            MFX_CHECK_STS(mfxRes);
            ConvertBitStreamMfx2Umc(*bs, mpeg2bs);
        }
        else if (code == GROUP_START_CODE)
        {
            UMC::Status umcRes = m_implUMC.UnpackGroupHeader(mpeg2bs, m_groupHead);
            MFX_CHECK_UMC_STS(umcRes);
        }
        else if (code == PICTURE_START_CODE)
        {
            UMC::Status umcRes = m_implUMC.UnpackPictureHeader(mpeg2bs, m_seqHead, m_picHead);
            MFX_CHECK_UMC_STS(umcRes);
            if ((m_picHead.picHeadQuantPresent) &&
                (m_picHead.load_iqm || m_picHead.load_niqm || m_picHead.load_ciqm || m_picHead.load_cniqm))
                m_qMatrixChanged = true;

            break;
        }
        else
        {
            SKIP_BITS_32(mpeg2bs.bs);
        }
    }

    m_sPar.MPEG2.SliceId = 0xffff;
    ConvertBitStreamUmc2Mfx(mpeg2bs, *bs);
    PopulateMfxFrameParam(m_fPar);
    if (par != &m_fPar)
        *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunSliceParam(mfxBitstream *bs, mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);
    BufferLocker locker(*m_core, *bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    UMC::Mpeg2Bitstream mpeg2bs;
    ConvertBitStreamMfx2Umc(*bs, mpeg2bs);

    // find picture header start code
    mfxU32 curSliceHeadOff = 0;
    mfxU32 curSliceHeadSize = 0;
    mfxStatus mfxRes = FindAndParseSlice(mpeg2bs, m_sliceHead, curSliceHeadOff, curSliceHeadSize);
    MFX_CHECK_STS(mfxRes);

    mfxU32 nextSliceHeadOff = 0;
    mfxU32 nextSliceHeadSize = 0;
    UMC::Mpeg2Slice nextSlice;
    mfxRes = FindAndParseSlice(mpeg2bs, nextSlice, nextSliceHeadOff, nextSliceHeadSize);
    if (mfxRes == MFX_ERR_MORE_DATA)
        return mfxRes;

    if (mfxRes != MFX_ERR_NONE || nextSlice.firstMbY != m_sliceHead.firstMbY)
        nextSlice.firstMbX = m_fPar.MPEG2.FrameWinMbMinus1 + 1; // last slice or new row of macroblocks
    VM_ASSERT(nextSlice.firstMbX > m_sliceHead.firstMbX);

    // map umc to mfx
    m_sPar.MPEG2.CucId = MFX_CUC_MPEG2_SLICEPARAM;
    m_sPar.MPEG2.CucSz = sizeof(mfxSliceParam);
    m_sPar.MPEG2.NumMb = (mfxU16)(nextSlice.firstMbX - m_sliceHead.firstMbX);
    m_sPar.MPEG2.FirstMbX = (mfxU8)m_sliceHead.firstMbX;
    m_sPar.MPEG2.FirstMbY = (mfxU8)m_sliceHead.firstMbY;
    m_sPar.MPEG2.BadSliceChopping = MFX_SLICECHOP_NONE;
    m_sPar.MPEG2.SliceType = GetMfxSliceTypeType(m_fPar.MPEG2.FrameType);
    m_sPar.MPEG2.QScaleCode = m_sliceHead.quantiser_scale_code;
    m_sPar.MPEG2.SliceId++;

    m_sPar.MPEG2.SliceDataOffset = curSliceHeadOff;
    m_sPar.MPEG2.SliceDataSize = nextSliceHeadOff - curSliceHeadOff;
    m_sPar.MPEG2.SliceHeaderSize = (mfxU16)curSliceHeadSize;

    bs->DataLength -= curSliceHeadOff - bs->DataOffset;
    bs->DataOffset = curSliceHeadOff;
    if (par != &m_sPar)
        *par = m_sPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunSliceBSD(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR3(cuc, cuc->MbParam, cuc->MbParam->Mb);
    MFX_CHECK_NULL_PTR2(cuc->Bitstream, cuc->SliceParam);
    BufferLocker locker(*m_core, *cuc->Bitstream);
    MFX_CHECK_NULL_PTR1(cuc->Bitstream->Data);

    UMC::Mpeg2Bitstream mpeg2bs;
    ConvertBitStreamMfx2Umc(*cuc->Bitstream, mpeg2bs);

    UMC::Mpeg2Slice tmp;
    UMC::Status umcRes = m_implUMC.UnpackSliceHeader(mpeg2bs, m_seqHead, tmp);
    MFX_CHECK_UMC_STS(umcRes);

    mfxExtMpeg2Coeffs* coeff = GetExtBuffer<mfxExtMpeg2Coeffs>(*cuc, MFX_CUC_MPEG2_RESPIXEL);
    MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);

    mfxStatus mfxSts = UpdateQMatrices(*cuc);
    MFX_CHECK_STS(mfxSts);

    UMC::Mpeg2Mb mb;
    mb.InitFirstMb(m_sliceHead, m_picHead);

    mfxU16 prevMbX = m_sPar.MPEG2.FirstMbX - 1;
    mfxMbCode* pMfxMbBase = GetFirstMfxMbCode(*cuc);
    for (mfxU32 i = 0; i < m_sPar.MPEG2.NumMb; i++)
    {
        UMC::Status umcRes = m_implUMC.UnpackMacroblock(mpeg2bs, m_seqHead, m_picHead, mb);
        MFX_CHECK_UMC_STS(umcRes);
        VM_ASSERT(mb.mbX - prevMbX > 1 && i != 0);

        if (i != 0)
            for (mfxI32 j = 0; j < mb.mbX - prevMbX - 1 && i < m_sPar.MPEG2.NumMb; j++, i++)
                PopulateMfxMbCodeSkipped(pMfxMbBase[i - 1], pMfxMbBase[i]);

        mb.dctCoef = GetDctCoefPtr(coeff->Coeffs, mb.mbX, mb.mbY, m_fPar);
        umcRes = mb.IsIntra() ?
            m_implUMC.UnpackIntraMacroblockCoef(mpeg2bs, m_picHead.intra_vlc_format, m_seqHead.chroma_format, mb) :
            m_implUMC.UnpackInterMacroblockCoef(mpeg2bs, m_seqHead.chroma_format, mb);

        mfxMbCode& mfxMb = pMfxMbBase[i];
        PopulateMfxMbCode(mb, mfxMb);
        prevMbX = mb.mbX;
    }

    ConvertBitStreamUmc2Mfx(mpeg2bs, *cuc->Bitstream);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunFrameBSD(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR3(cuc, cuc->SliceParam, cuc->Bitstream);
    BufferLocker locker(*m_core, *cuc->Bitstream);
    MFX_CHECK_NULL_PTR1(cuc->Bitstream->Data);

    mfxBitstream copy = *cuc->Bitstream;
    mfxStatus mfxSts = MFX_ERR_NONE;
    for (cuc->SliceId = 0;; ++cuc->SliceId)
    {
        mfxSts = RunSliceParam(cuc->Bitstream, &cuc->SliceParam[cuc->SliceId]);
        if (mfxSts != MFX_ERR_NONE)
        {
            if (mfxSts == MFX_ERR_NULL_PTR)
                mfxSts = MFX_ERR_NONE; // no more slices
            break;
        }

        mfxSts = RunSliceBSD(cuc);
        if (mfxSts != MFX_ERR_NONE)
            break;
    }

    cuc->NumSlice = cuc->SliceId;
    cuc->SliceId = 0;

    if (mfxSts != MFX_ERR_NONE)
        *cuc->Bitstream = copy; // not enough data, restore bitstream

    return mfxSts;
}

mfxStatus VideoBSDMPEG2::RunSliceMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::RunFrameMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::ExtractUserData(mfxBitstream *bs, mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    MFX_CHECK_NULL_PTR3(ud, sz, ts);
    bs;

    UMC::MediaData cc;
    UMC::Status umcRes = m_implUMC.GetUserData(&cc);
    MFX_CHECK_UMC_STS(umcRes);

    // it is assumed that pointer 'ud' is allocated and 'sz' contains size of allocated buffer
    // check if allocated buffer is enough to store user data
    if (*sz < (mfxU32)cc.GetDataSize())
    {
        *sz = (mfxU32)cc.GetDataSize();
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    memcpy(ud, cc.GetDataPointer(), cc.GetDataSize());
    *sz = (mfxU32)cc.GetDataSize();
    *ts = (mfxU64)(cc.GetTime() * MFX_TIME_STAMP_FACTOR);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::InternalReset(mfxVideoParam* par)
{
    m_vPar = *par;
    m_vPar.mfx.FrameInfo.CropW = 0;
    m_vPar.mfx.FrameInfo.CropH = 0;
    ClearMfxStruct(&m_fPar);
    ClearMfxStruct(&m_sPar);
    m_qMatrixChanged = true;

    if (!m_bVlcTableInited)
    {
        m_bVlcTableInited = m_implUMC.InitTables();
        if (!m_bVlcTableInited)
            return ConvertStatusUmc2Mfx(UMC::UMC_ERR_INIT);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDMPEG2::FindAndParseSlice(UMC::Mpeg2Bitstream& bs, UMC::Mpeg2Slice& slice, mfxU32& headOffset, mfxU32& headSize)
{
    mfxU32 code;
    mfxU8 firstMbX;
    mfxU32 offset;
    mfxU32 size;

    for (;;)
    {
        FIND_START_CODE(bs.bs, code);
        if (code == UMC::UMC_ERR_NOT_ENOUGH_DATA)
            return ConvertStatusUmc2Mfx(UMC::UMC_ERR_NOT_ENOUGH_DATA);
        if (!(0x101 <= code && code <= 0x1af))
            return MFX_ERR_NULL_PTR;

        offset = (mfxU32)(bs.bs_curr_ptr - bs.bs_start_ptr);
        UMC::Status umcRes = m_implUMC.UnpackSliceHeader(bs, m_seqHead, slice);
        if (umcRes != UMC::UMC_OK && umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
            continue; // invalid stream

        size = (mfxU32)(bs.bs_curr_ptr - bs.bs_start_ptr) - offset;
        firstMbX = (mfxU8)m_implUMC.UnpackMbInc(bs);
        if (firstMbX == 0)
            continue; // invalid stream

        break; // slice found and parsed
    }

    headSize = size;
    headOffset = offset;
    slice.firstMbX = firstMbX - 1;
    return MFX_ERR_NONE;
}

void VideoBSDMPEG2::PopulateMfxVideoParam(mfxVideoParam& par)
{
    par.mfx.CodecProfile = m_seqHead.profile;
    par.mfx.CodecLevel = m_seqHead.level << 4;
   // par.mfx.Log2MaxFrameOrder = 8; // Must be either 8 or 16 for now.
    par.mfx.FrameInfo.CropW = (mfxU16)m_seqHead.horizontal_size;
    par.mfx.FrameInfo.CropH = (mfxU16)m_seqHead.vertical_size;
    par.mfx.FrameInfo.CropX = 0;
    par.mfx.FrameInfo.CropY = 0;
    par.mfx.FrameInfo.Width = AlignValue(par.mfx.FrameInfo.CropW, MFX_MB_WIDTH);
    par.mfx.FrameInfo.Height = AlignValue(par.mfx.FrameInfo.CropH, MFX_MB_WIDTH);
    par.mfx.FrameInfo.PicStruct = m_seqHead.progressive_sequence ?
        (mfxU8)MFX_PICSTRUCT_PROGRESSIVE :
        (mfxU8)MFX_PICSTRUCT_FIELD_TFF; // actually first picture is needed to know exactly
    par.mfx.FrameInfo.ChromaFormat = Mpeg2GetMfxChromaFormat(m_seqHead.chroma_format);
  //  par.mfx.FrameInfo.AspectRatio = m_seqHead.aspect_ratio_information;
    ////par.mfx.FrameInfo.FrameRateCode = GetMfxFrameRateCode(m_seqHead.frame_rate_code);
    ////par.mfx.FrameInfo.FrameRateExtN = m_seqHead.frame_rate_extension_n;
    ////par.mfx.FrameInfo.FrameRateExtD = m_seqHead.frame_rate_extension_d;
    GetMfxFrameRate(m_seqHead.frame_rate_code, &par.mfx.FrameInfo.FrameRateExtN, &par.mfx.FrameInfo.FrameRateExtD);
    par.mfx.FrameInfo.FrameRateExtN *= m_seqHead.frame_rate_extension_n + 1;
    par.mfx.FrameInfo.FrameRateExtD = m_seqHead.frame_rate_extension_d;
   // par.mfx.FrameInfo.BitDepthLuma = 8; // Must be 8 for both for now.
   // par.mfx.FrameInfo.BitDepthChroma = 8; // Must be 8 for both for now.
    par.mfx.TargetKbps = (mfxU16)(m_seqHead.bitrate / 1000);
    par.mfx.BufferSizeInKB = (mfxU16)(m_seqHead.vbv_buffer_size / 1024);
}

void VideoBSDMPEG2::PopulateMfxFrameParam(mfxFrameParam& par)
{
    par.MPEG2.CucId = MFX_CUC_MPEG2_FRAMEPARAM;
    par.MPEG2.CucSz = sizeof(mfxFrameParam);
    par.MPEG2.FrameType = GetMfxFrameType(m_picHead.picture_coding_type, m_picHead.picture_structure);
    par.MPEG2.DecFrameLabel; // do not touch here?
    par.MPEG2.VppFrameLabel; // do not touch here?
    par.MPEG2.CurrFrameLabel; // do not touch here?
    par.MPEG2.FrameWinMbMinus1 = ((m_seqHead.horizontal_size + 15) >> 4) - 1;
    par.MPEG2.FrameHinMbMinus1 = ((m_seqHead.vertical_size + 15) >> 4) - 1;;
    par.MPEG2.NumMb = (par.MPEG2.FrameWinMbMinus1 + 1) * (par.MPEG2.FrameHinMbMinus1 + 1);
    par.MPEG2.SecondFieldFlag = GetMfxSecondFieldFlag(
        m_picHead.picture_structure, par.MPEG2.FieldPicFlag, par.MPEG2.BottomFieldFlag, par.MPEG2.SecondFieldFlag);
    par.MPEG2.FieldPicFlag = (m_picHead.picture_structure != FRAME_PICTURE);
    par.MPEG2.InterlacedFrameFlag = 0; // unclear
    par.MPEG2.BottomFieldFlag = (m_picHead.picture_structure == BOTTOM_FIELD);
    par.MPEG2.ChromaFormatIdc = m_seqHead.chroma_format;
    par.MPEG2.RefPicFlag = (m_picHead.picture_coding_type != UMC::B_PICTURE);
    par.MPEG2.BackwardPredFlag = (m_picHead.picture_coding_type == UMC::B_PICTURE);
    par.MPEG2.NoResidDiffs = 0; // unclear, depends on entry point?
    par.MPEG2.BrokenLinkFlag = m_groupHead.broken_link;
    par.MPEG2.CloseEntryFlag = m_groupHead.closed_gop;
   // par.MPEG2.ScanMethod = m_picHead.alternate_scan;
    par.MPEG2.NumRefFrame = GetMfxNumRefFrame(m_picHead.picture_coding_type);
    par.MPEG2.IntraPicFlag = (m_picHead.picture_coding_type == UMC::I_PICTURE);
    par.MPEG2.BitStreamFcodes = GetMfxBitStreamFcodes(m_picHead.f_code);

    par.MPEG2.TemporalReference = m_picHead.temporal_reference;
    par.MPEG2.ProgressiveFrame = m_picHead.progressive_frame;
    par.MPEG2.Chroma420type = 0;
    par.MPEG2.RepeatFirstField = m_picHead.repeat_first_field;
    par.MPEG2.AlternateScan = m_picHead.alternate_scan;
    par.MPEG2.IntraVLCformat = m_picHead.intra_vlc_format;
    par.MPEG2.QuantScaleType = m_picHead.q_scale_type;
    par.MPEG2.ConcealmentMVs = m_picHead.concealment_motion_vectors;
    par.MPEG2.FrameDCTprediction = m_picHead.frame_pred_frame_dct;
    par.MPEG2.TopFieldFirst = m_picHead.top_field_first;
    par.MPEG2.PicStructure = m_picHead.picture_structure;
    par.MPEG2.IntraDCprecision = m_picHead.intra_dc_precision;
}

void VideoBSDMPEG2::PopulateMfxMbCodeSkipped(const mfxMbCode& prevMb, mfxMbCode& mfxMb)
{
    mfxMb = prevMb;
    mfxMb.MPEG2.FirstMbFlag = 0;
    mfxMb.MPEG2.LastMbFlag = 0;
    mfxMb.MPEG2.Skip8x8Flag = 0xf;

    if (prevMb.MPEG2.MbType5Bits == B_16x8_Field_L2L2 || prevMb.MPEG2.MbType5Bits == B_16x16_Bi)
        mfxMb.MPEG2.MbType5Bits = B_Skip_Bi;
    else if (prevMb.MPEG2.MbType5Bits == B_16x8_Field_L1L1 || prevMb.MPEG2.MbType5Bits == B_16x16_L1)
        mfxMb.MPEG2.MbType5Bits = B_Skip_L1;
    else
        mfxMb.MPEG2.MbType5Bits = PB_Skip_L0;

    mfxMb.MPEG2.IntraMbFlag = 0;
    mfxMb.MPEG2.NumPackedMv = 0;
    mfxMb.MPEG2.MvUnpackedFlag = 1;
    mfxMb.MPEG2.MbXcnt = prevMb.MPEG2.MbXcnt + 1;
    mfxMb.MPEG2.MbDataSize128b = 0;
    mfxMb.MPEG2.MbDataOffset = 0;
    mfxMb.MPEG2.CodedPattern4x4Y = 0;
    mfxMb.MPEG2.CodedPattern4x4U = 0;
    mfxMb.MPEG2.CodedPattern4x4V = 0;
}

void VideoBSDMPEG2::PopulateMfxMbCode(const UMC::Mpeg2Mb& umcMb, mfxMbCode& mfxMb)
{
    mfxMb.MPEG2.Skip8x8Flag = 0;
    mfxMb.MPEG2.MbType5Bits = GetMfxMbType5bits(m_picHead.picture_structure, umcMb.macroblock_type, umcMb.motion_type);
    mfxMb.MPEG2.FieldMbFlag = GetMfxFieldMbFlag(m_picHead.picture_structure, umcMb.motion_type);
    mfxMb.MPEG2.IntraMbFlag = umcMb.IsIntra();
    mfxMb.MPEG2.TransformFlag = umcMb.dct_type ? 1 : 0;
    mfxMb.MPEG2.NumPackedMv = 0;//GetMfxMvQuantity(m_picHead.picture_structure, umcMb.macroblock_type, umcMb.motion_type);
    mfxMb.MPEG2.MvUnpackedFlag = 1;
    mfxMb.MPEG2.MbXcnt = (mfxU8)umcMb.mbX;
    mfxMb.MPEG2.MbYcnt = (mfxU8)umcMb.mbY;

    mfxMb.MPEG2.FirstMbFlag = (mfxMb.MPEG2.MbXcnt == m_sPar.MPEG2.FirstMbX);
    mfxMb.MPEG2.LastMbFlag = (mfxMb.MPEG2.MbXcnt == m_sPar.MPEG2.FirstMbX + m_sPar.MPEG2.NumMb - 1);

   // mfxMb.MPEG2.RefPicSelect[0][0] = umcMb.field_select[UMC::Mpeg2MvR0Fwd];
  //  mfxMb.MPEG2.RefPicSelect[0][1] = umcMb.field_select[UMC::Mpeg2MvR1Fwd];
  //  mfxMb.MPEG2.RefPicSelect[1][0] = umcMb.field_select[UMC::Mpeg2MvR0Bwd];
  //  mfxMb.MPEG2.RefPicSelect[1][1] = umcMb.field_select[UMC::Mpeg2MvR1Bwd];
    mfxMb.MPEG2.MV[0][0] = umcMb.pmv[UMC::Mpeg2MvR0Fwd].x;
    mfxMb.MPEG2.MV[0][1] = umcMb.pmv[UMC::Mpeg2MvR0Fwd].y;
    mfxMb.MPEG2.MV[1][0] = umcMb.pmv[UMC::Mpeg2MvR1Fwd].x;
    mfxMb.MPEG2.MV[1][1] = umcMb.pmv[UMC::Mpeg2MvR1Fwd].y;
    mfxMb.MPEG2.MV[2][0] = umcMb.mv [UMC::Mpeg2MvR0Fwd].x;
    mfxMb.MPEG2.MV[2][1] = umcMb.mv [UMC::Mpeg2MvR0Fwd].y;
    mfxMb.MPEG2.MV[3][0] = umcMb.mv [UMC::Mpeg2MvR1Fwd].x;
    mfxMb.MPEG2.MV[3][1] = umcMb.mv [UMC::Mpeg2MvR1Fwd].y;

    mfxMb.MPEG2.MV[4][0] = umcMb.pmv[UMC::Mpeg2MvR0Bwd].x;
    mfxMb.MPEG2.MV[4][1] = umcMb.pmv[UMC::Mpeg2MvR0Bwd].y;
    mfxMb.MPEG2.MV[5][0] = umcMb.pmv[UMC::Mpeg2MvR1Bwd].x;
    mfxMb.MPEG2.MV[5][1] = umcMb.pmv[UMC::Mpeg2MvR1Bwd].y;
    mfxMb.MPEG2.MV[6][0] = umcMb.mv [UMC::Mpeg2MvR0Bwd].x;
    mfxMb.MPEG2.MV[6][1] = umcMb.mv [UMC::Mpeg2MvR0Bwd].y;
    mfxMb.MPEG2.MV[7][0] = umcMb.mv [UMC::Mpeg2MvR1Bwd].x;
    mfxMb.MPEG2.MV[7][1] = umcMb.mv [UMC::Mpeg2MvR1Bwd].y;

    mfxMb.MPEG2.MbScanMethod = m_picHead.alternate_scan;//m_fPar.MPEG2.ScanMethod;
    mfxMb.MPEG2.QpScaleType = m_picHead.q_scale_type;
    mfxMb.MPEG2.QpScaleCode = umcMb.quantiser_scale_code;

    mfxMb.MPEG2.CodedPattern4x4Y = GetMfxCodedPattern4x4Y(umcMb.coded_block_pattern, m_fPar.MPEG2.ChromaFormatIdc);
    mfxMb.MPEG2.CodedPattern4x4U = GetMfxCodedPattern4x4U(umcMb.coded_block_pattern, m_fPar.MPEG2.ChromaFormatIdc);
    mfxMb.MPEG2.CodedPattern4x4V = GetMfxCodedPattern4x4V(umcMb.coded_block_pattern, m_fPar.MPEG2.ChromaFormatIdc);

  //  mfxMb.MPEG2.SubMbShape = 0;
    mfxMb.MPEG2.SubMbPredMode = 0;
  //  mfxMb.MPEG2.SubMbShapeU = 0;
  //  mfxMb.MPEG2.SubMbShapeV = 0;
}

mfxStatus VideoBSDMPEG2::UpdateQMatrices(mfxFrameCUC& cuc)
{
    if (m_qMatrixChanged)
    {
        mfxExtMpeg2QMatrix* qm = GetExtBuffer<mfxExtMpeg2QMatrix>(cuc, MFX_CUC_MPEG2_QMATRIX);
        MFX_CHECK_NULL_PTR1(qm);
        const Ipp8u* intraQm = m_picHead.load_iqm ? m_picHead.iqm : m_seqHead.load_iqm ? m_seqHead.iqm : 0;
        const Ipp8u* interQm = m_picHead.load_niqm ? m_picHead.niqm : m_seqHead.load_niqm ? m_seqHead.niqm : 0;
        m_implUMC.DeZigzagIntraQMatrix(intraQm, qm->IntraQMatrix);
        m_implUMC.DeZigzagInterQMatrix(interQm, qm->InterQMatrix);
        m_qMatrixChanged = false;
    }

    return MFX_ERR_NONE;
}

#endif
