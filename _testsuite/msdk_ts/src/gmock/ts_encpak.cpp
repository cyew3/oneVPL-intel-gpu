/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encpak.h"

//#include "sample_utils.h"
// for
mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId);

// there should be common function like this instead of one in mfx_h264_enc_common_hw.h
mfxExtBuffer* GetExtFeiBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId, mfxU32 field)
{
    for (mfxU16 i = 0; i < nbuffers; i++)
         if (ebuffers[i] != 0 && ebuffers[i]->BufferId == BufferId && !field --) // skip "field" times
            return (ebuffers[i]);
    return 0;
}

mfxExtBuffer* GetExtFeiBuffer(std::vector<mfxExtBuffer*>& buff, mfxU32 BufferId, mfxU32 field)
{
    for (std::vector<mfxExtBuffer*>::iterator it = buff.begin(); it != buff.end(); it++) {
        if (*it && (*it)->BufferId == BufferId && !field --)
            return *it;
    }
    return 0;
}


mfxStatus PrepareCtrlBuf(mfxExtFeiEncFrameCtrl* & fei_encode_ctrl, const mfxVideoParam& vpar)
{
    mfxU32 nfields = (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    if (!fei_encode_ctrl)
        fei_encode_ctrl = (mfxExtFeiEncFrameCtrl*) new mfxU8 [sizeof(mfxExtFeiEncFrameCtrl) * nfields];
    memset(fei_encode_ctrl, 0, sizeof(*fei_encode_ctrl) * nfields);

    for (mfxU32 field = 0; field < nfields; field++)
    {
        fei_encode_ctrl[field].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        fei_encode_ctrl[field].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

        fei_encode_ctrl[field].SearchPath             = 2;
        fei_encode_ctrl[field].LenSP                  = 57;
        fei_encode_ctrl[field].SubMBPartMask          = 0;
        fei_encode_ctrl[field].MultiPredL0            = 0;
        fei_encode_ctrl[field].MultiPredL1            = 0;
        fei_encode_ctrl[field].SubPelMode             = 0x03;
        fei_encode_ctrl[field].InterSAD               = 0;
        fei_encode_ctrl[field].IntraSAD               = 0;
        fei_encode_ctrl[field].IntraPartMask          = 0;
        fei_encode_ctrl[field].DistortionType         = 0;
        fei_encode_ctrl[field].RepartitionCheckEnable = 0;
        fei_encode_ctrl[field].AdaptiveSearch         = 0;
        fei_encode_ctrl[field].MVPredictor            = 0;
        fei_encode_ctrl[field].NumMVPredictors[0]     = 0;
        fei_encode_ctrl[field].PerMBQp                = 26;
        fei_encode_ctrl[field].PerMBInput             = 0;
        fei_encode_ctrl[field].MBSizeCtrl             = 0;
        fei_encode_ctrl[field].ColocatedMbDistortion  = 0;
        fei_encode_ctrl[field].RefHeight              = 32;
        fei_encode_ctrl[field].RefWidth               = 32;
        fei_encode_ctrl[field].SearchWindow           = 5;
    }

    return MFX_ERR_NONE;
}

mfxStatus PrepareSPSBuf(mfxExtFeiSPS* & fsps, const mfxVideoParam& vpar)
{
    if (!fsps)
        fsps = (mfxExtFeiSPS*) new mfxU8 [sizeof(mfxExtFeiSPS)];

    memset(fsps, 0, sizeof(*fsps));
    fsps->Header.BufferId = MFX_EXTBUFF_FEI_SPS;
    fsps->Header.BufferSz = sizeof(mfxExtFeiSPS);

    fsps->SPSId = DEFAULT_IDC;
    fsps->PicOrderCntType = 0;
    fsps->Log2MaxPicOrderCntLsb = 4;

    return MFX_ERR_NONE;
}

mfxStatus PreparePPSBuf(mfxExtFeiPPS* & fpps, const mfxVideoParam& vpar)
{
    mfxU32 nfields = (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    if (!fpps)
        fpps = (mfxExtFeiPPS*) new mfxU8 [sizeof(mfxExtFeiPPS) * nfields];

    memset(fpps, 0, sizeof(*fpps));
    fpps->Header.BufferId = MFX_EXTBUFF_FEI_PPS;
    fpps->Header.BufferSz = sizeof(mfxExtFeiPPS);

    fpps->SPSId = DEFAULT_IDC;
    fpps->PPSId = DEFAULT_IDC;

    fpps->FrameType = MFX_FRAMETYPE_I;
    fpps->PictureType = MFX_PICTYPE_FRAME;

    fpps->PicInitQP = 26;
    fpps->NumRefIdxL0Active = 0;
    fpps->NumRefIdxL1Active = 0;
    for(int i=0; i<16; i++) {
        fpps->DpbBefore[i].Index = 0xffff;
        fpps->DpbBefore[i].PicType = 0;
        fpps->DpbBefore[i].FrameNumWrap = 0;
        fpps->DpbBefore[i].LongTermFrameIdx = 0;
        fpps->DpbAfter[i] = fpps->DpbBefore[i];
    }

    //fpps->ChromaQPIndexOffset;
    //fpps->SecondChromaQPIndexOffset;

    fpps->Transform8x8ModeFlag = 1;

    if (nfields == 2) // the same for 2nd field
        fpps[1] = fpps[0];

    return MFX_ERR_NONE;
}

mfxStatus PrepareParamBuf(mfxExtFeiParam* & fpar, const mfxVideoParam& vpar, mfxFeiFunction func)
{
    if (!fpar)
        fpar = (mfxExtFeiParam*) new mfxU8 [sizeof(mfxExtFeiParam)];

    memset(fpar, 0, sizeof(*fpar));
    fpar->Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
    fpar->Header.BufferSz = sizeof(mfxExtFeiParam);
    fpar->Func = func;
    fpar->SingleFieldProcessing = (vpar.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON;

    return MFX_ERR_NONE;
}


mfxStatus PrepareSliceBuf(mfxExtFeiSliceHeader* & fslice, const mfxVideoParam& vpar)
{
    mfxU32 nfields = (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    if (!fslice)
        //fslice = new mfxExtFeiSliceHeader;
        fslice = (mfxExtFeiSliceHeader*) new mfxU8 [sizeof(mfxExtFeiSliceHeader) * nfields];
    else
        SAFE_DELETE_ARRAY (fslice->Slice)

    memset(fslice, 0, sizeof(*fslice) * nfields);
    for (mfxU32 field = 0; field < nfields; field++)
    {
        fslice[field].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
        fslice[field].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

        mfxI32 wmb = (vpar.mfx.FrameInfo.Width+15)>>4;
        mfxI32 hmb = (vpar.mfx.FrameInfo.Height+15)>>4;
        if (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
            hmb = (hmb+1) >> 1;
        mfxI32 nummb = wmb*hmb;
        mfxU16 numSlice = vpar.mfx.NumSlice;
        if (numSlice == 0 || numSlice > nummb) numSlice = 1;

        fslice[field].Slice = new mfxExtFeiSliceHeader::mfxSlice[numSlice];
        memset(fslice[field].Slice, 0, sizeof(*fslice[field].Slice) * numSlice);
        fslice[field].NumSlice = numSlice;

        mfxU16 firstmb = 0;
        for (mfxI32 s = 0; s < numSlice; s++) {
            mfxExtFeiSliceHeader::mfxSlice &slice = fslice[field].Slice[s];
            slice.SliceType = 2; //FEI_SLICETYPE_I;
            slice.PPSId = DEFAULT_IDC;
            slice.IdrPicId = DEFAULT_IDC;
            slice.CabacInitIdc = 0;
            slice.NumRefIdxL0Active = 0;
            slice.NumRefIdxL1Active = 0;
            slice.SliceQPDelta = 0;
            slice.DisableDeblockingFilterIdc = 0;
            slice.SliceAlphaC0OffsetDiv2 = 0;
            slice.SliceBetaOffsetDiv2 = 0;
            // RefL0/L1 ignored

            slice.MBAddress = firstmb;
            mfxI32 lastmb = nummb*(s+1)/numSlice;
            slice.NumMBs = lastmb - firstmb;
            firstmb = lastmb;
            if (firstmb >= nummb) {
                fslice[field].NumSlice = numSlice = s+1;
                break;
            }
        }
        fslice[field].Slice[numSlice-1].NumMBs = nummb - fslice[field].Slice[numSlice-1].MBAddress; // eon of last slice is last MB
    }

    return MFX_ERR_NONE;
}

mfxStatus PrepareEncMVBuf(mfxExtFeiEncMV & mv, const mfxVideoParam& vpar)
{
    SAFE_DELETE_ARRAY(mv.MB);
    memset(&mv, 0, sizeof(mv));
    mv.Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
    mv.Header.BufferSz = sizeof(mfxExtFeiEncMV);

    mfxI32 wmb = (vpar.mfx.FrameInfo.Width+15)>>4;
    mfxI32 hmb = (vpar.mfx.FrameInfo.Height+15)>>4;
    if (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        hmb = (hmb+1) >> 1;
    mfxI32 nummb = wmb*hmb;

    mv.MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[nummb];
    memset(mv.MB, 0, sizeof(*mv.MB) * nummb);
    mv.NumMBAlloc = nummb;

    return MFX_ERR_NONE;
}

mfxStatus PreparePakMBCtrlBuf(mfxExtFeiPakMBCtrl & mb, const mfxVideoParam& vpar)
{
    SAFE_DELETE_ARRAY(mb.MB);
    memset(&mb, 0, sizeof(mb));
    mb.Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
    mb.Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);

    mfxI32 wmb = (vpar.mfx.FrameInfo.Width+15)>>4;
    mfxI32 hmb = (vpar.mfx.FrameInfo.Height+15)>>4;
    if (vpar.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        hmb = (hmb+1) >> 1;
    mfxI32 nummb = wmb*hmb;

    mb.MB = new mfxFeiPakMBCtrl[nummb];
    memset(mb.MB, 0, sizeof(*mb.MB) * nummb);
    mb.NumMBAlloc = nummb;

    return MFX_ERR_NONE;
}

// The function deletes buffers and zeroes pointers in array. Todo get rid of it
mfxStatus tsVideoENCPAK::Close()
{
    m_enc_pool.FreeSurfaces();
    m_rec_pool.FreeSurfaces();

    SAFE_DELETE_ARRAY(m_bitstream.Data);
    mfxU32 nfields = (enc.m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    for (mfxU32 field = 0; field < nfields; field++) {
        SAFE_DELETE_ARRAY(mb[field].MB);
        SAFE_DELETE_ARRAY(mv[field].MB);
    }

    SAFE_DELETE_ARRAY(enc.fctrl);
    SAFE_DELETE_ARRAY(enc.fpps);
    SAFE_DELETE_ARRAY(enc.fsps);
    SAFE_DELETE_ARRAY(enc.fpar);
    if (enc.fslice) {
        SAFE_DELETE_ARRAY(enc.fslice->Slice);
        SAFE_DELETE_ARRAY(enc.fslice);
    }

    SAFE_DELETE_ARRAY(pak.fctrl);
    SAFE_DELETE_ARRAY(pak.fpps);
    SAFE_DELETE_ARRAY(pak.fsps);
    SAFE_DELETE_ARRAY(pak.fpar);
    if (pak.fslice) {
        SAFE_DELETE_ARRAY(pak.fslice->Slice);
        SAFE_DELETE_ARRAY(pak.fslice);
    }

    m_initialized = false;
    m_frames_buffered = 0;

    return Close(m_session);
}

mfxStatus ReleaseExtBufs (std::vector<mfxExtBuffer*>& in_buffs, mfxU32 num_fields)
{
    for (std::vector<mfxExtBuffer*>::iterator it = in_buffs.begin(); it != in_buffs.end(); it++) {
        if (!*it)
            continue;
        switch ((*it)->BufferId) {
            case MFX_EXTBUFF_FEI_ENC_CTRL:
                SAFE_DELETE_ARRAY(*it);
                for (mfxU32 i = 1; i < num_fields; i++) {
                    ++it;
                    *it = NULL;
                }
                break;

            case MFX_EXTBUFF_FEI_SPS:
                SAFE_DELETE_ARRAY(*it);
                break;

            case MFX_EXTBUFF_FEI_PPS:
                SAFE_DELETE_ARRAY(*it);
                break;

            case MFX_EXTBUFF_FEI_PARAM:
                SAFE_DELETE_ARRAY(*it);
                break;

            case MFX_EXTBUFF_FEI_SLICE:
            {
                mfxExtFeiSliceHeader* fslice = (mfxExtFeiSliceHeader*)(*it);
                SAFE_DELETE_ARRAY(fslice->Slice);
                SAFE_DELETE_ARRAY(*it);
                break;
            }
            case MFX_EXTBUFF_FEI_ENC_MV:
            {
                mfxExtFeiEncMV* mv = (mfxExtFeiEncMV*)(*it);
                SAFE_DELETE_ARRAY(mv->MB);
                break;
            }
            case MFX_EXTBUFF_FEI_PAK_CTRL:
            {
                mfxExtFeiPakMBCtrl* mb = (mfxExtFeiPakMBCtrl*)(*it);
                SAFE_DELETE_ARRAY(mb->MB);
                break;
            }
            default:
                break;
        }
    }
    return MFX_ERR_NONE;
}

tsVideoENCPAK::tsVideoENCPAK(mfxFeiFunction funcEnc, mfxFeiFunction funcPak, mfxU32 CodecId, bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_loaded(false)
    , enc(funcEnc)
    , pak(funcPak)
    , m_enc_pool()
    , m_rec_pool()
    , m_enc_request()
    , m_rec_request()
    , m_bitstream()
    , m_pBitstream(&m_bitstream)
    , m_bs_processor(0)
    , m_frames_buffered(0)
    , m_pSyncPoint(&m_syncpoint)
    , m_filler(0)
    , m_uid(0)
    , m_ENCInput(&ENCInput)
    , m_ENCOutput(&ENCOutput)
    , m_PAKInput(&PAKInput)
    , m_PAKOutput(&PAKOutput)
{
    memset(&ENCInput, 0, sizeof(ENCInput));
    memset(&ENCOutput, 0, sizeof(ENCOutput));
    memset(&PAKInput, 0, sizeof(PAKInput));
    memset(&PAKOutput, 0, sizeof(PAKOutput));
    memset(&mb, 0, sizeof(mb));
    memset(&mv, 0, sizeof(mv));

    if(m_default)
    {
        enc.m_par.mfx.FrameInfo.Width  = enc.m_par.mfx.FrameInfo.CropW = 720;
        enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 576;
        enc.m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        enc.m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        enc.m_par.mfx.FrameInfo.FrameRateExtN = 30;
        enc.m_par.mfx.FrameInfo.FrameRateExtD = 1;
        enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26;
        enc.m_par.IOPattern        = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        enc.m_par.mfx.EncodedOrder = 1;
        enc.m_par.AsyncDepth       = 1;

        pak.m_par.mfx = enc.m_par.mfx;
        pak.m_par.IOPattern = enc.m_par.IOPattern;
        pak.m_par.mfx.EncodedOrder = 1;
        pak.m_par.AsyncDepth       = 1;
    }

    pak.m_par.mfx.CodecId = enc.m_par.mfx.CodecId = CodecId;

    m_loaded = true;
}

tsVideoENCPAK::~tsVideoENCPAK()
{
    if(m_initialized)
    {
        Close();
    }
}

mfxStatus tsVideoENCPAK::Init()
{
    if(m_default)
    {
        if (!m_session)
        {
            MFXInit();TS_CHECK_MFX;
        }
        if (!m_loaded)
        {
            Load();
        }

        m_BaseAllocID = (mfxU64)&m_session & 0xffffffff;
        m_EncPakReconAllocID = m_BaseAllocID + 0; // TODO remove this var

        enc.m_par.AllocId = m_EncPakReconAllocID;
        pak.m_par.AllocId = m_EncPakReconAllocID;

        if (!m_pFrameAllocator
            && (   (m_enc_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (enc.m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            m_pFrameAllocator = m_pVAHandle;
            SetFrameAllocator(); TS_CHECK_MFX;
            m_enc_pool.SetAllocator(m_pFrameAllocator, true);
            m_rec_pool.SetAllocator(m_pFrameAllocator, true);
        }
//        if (enc.m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) // do we need the same for pak?
//        {
//            QueryIOSurf();
//            m_enc_pool.AllocOpaque(m_enc_request, enc.osa);
//        }
    }

    AllocSurfaces();

    enc.m_par.ExtParam     = enc.initbuf.data();
    enc.m_par.NumExtParam  = (mfxU16)enc.initbuf.size();
    pak.m_par.ExtParam     = pak.initbuf.data();
    pak.m_par.NumExtParam  = (mfxU16)pak.initbuf.size();

    mfxStatus sts = Init(m_session);
    if (sts >= MFX_ERR_NONE) {
        mfxU32 nfields = (enc.m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
        for (mfxU32 field = 0; field < nfields; field++) {
            PreparePakMBCtrlBuf(mb[field], *enc.m_pPar);
            PrepareEncMVBuf(mv[field], *enc.m_pPar);
        }

        m_initialized = true;
        GetVideoParam(); // to get modifications if any
    }

    return sts;
}

mfxStatus tsVideoENCPAK::Init(mfxSession session/*, mfxVideoParam *par*/)
{
    mfxVideoParam orig_par, *par;

    // ENC
    par = enc.m_pPar;
    if (par)
        memcpy(&orig_par, par, sizeof(mfxVideoParam));

    TRACE_FUNC2(MFXVideoENC_Init, session, par);
    g_tsStatus.check( MFXVideoENC_Init(session, par) );

    mfxStatus enc_status = g_tsStatus.get();

    if (par)
        EXPECT_EQ(0, memcmp(&orig_par, par, sizeof(mfxVideoParam)))
            << "ERROR: Input parameters must not be changed in Enc Init()";

    if (enc_status < 0) return enc_status;

    // PAK
    par = pak.m_pPar;
    if (par)
        memcpy(&orig_par, par, sizeof(mfxVideoParam));

    TRACE_FUNC2(MFXVideoPAK_Init, session, par);
    g_tsStatus.check( MFXVideoPAK_Init(session, par) );

    mfxStatus pak_status = g_tsStatus.get();

    if (par)
        EXPECT_EQ(0, memcmp(&orig_par, par, sizeof(mfxVideoParam)))
            << "ERROR: Input parameters must not be changed in Pak Init()";

    if (pak_status < 0) return pak_status;

    return enc_status == 0 ? pak_status : enc_status; // first warning
}

mfxStatus tsVideoENCPAK::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoENC_Close, session);
    g_tsStatus.check( MFXVideoENC_Close(session) );

    TRACE_FUNC1(MFXVideoPAK_Close, session);
    g_tsStatus.check( MFXVideoPAK_Close(session) );

    return g_tsStatus.get();
}

mfxStatus tsVideoENCPAK::Query()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }

    //return Query(m_session, m_pPar, m_pParOut);
    TRACE_FUNC3(MFXVideoENC_Query, m_session, enc.m_pPar, enc.m_pParOut);
    g_tsStatus.check( MFXVideoENC_Query(m_session, enc.m_pPar, enc.m_pParOut) );

    // check if PAK is needed
    TRACE_FUNC3(MFXVideoPAK_Query, m_session, pak.m_pPar, pak.m_pParOut);
    g_tsStatus.check( MFXVideoPAK_Query(m_session, pak.m_pPar, pak.m_pParOut) );

    TS_TRACE(enc.m_pParOut);
    TS_TRACE(pak.m_pParOut);

    return g_tsStatus.get();
}


mfxStatus tsVideoENCPAK::QueryIOSurf()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }
    //return QueryIOSurf(m_session, enc.m_pPar, m_pRequest);
    m_enc_request.AllocId           = m_BaseAllocID;
    m_enc_request.Info              = enc.m_pPar->mfx.FrameInfo;
    m_enc_request.NumFrameMin       = enc.m_pPar->mfx.GopRefDist + enc.m_pPar->AsyncDepth; // what if !progressive?
    m_enc_request.NumFrameSuggested = m_enc_request.NumFrameMin;
    m_enc_request.Type              = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    m_enc_request.Type              |= (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK);
    TS_TRACE(m_enc_request);

    m_rec_request.AllocId           = m_EncPakReconAllocID;
    m_rec_request.Info              = pak.m_pPar->mfx.FrameInfo;
    m_rec_request.NumFrameMin       = pak.m_pPar->mfx.GopRefDist * 2 + (pak.m_pPar->AsyncDepth-1) + 1 + pak.m_pPar->mfx.NumRefFrame + 1;
    m_rec_request.NumFrameSuggested = m_rec_request.NumFrameMin;
    m_rec_request.Type              = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    m_rec_request.Type              |= (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK);
    TS_TRACE(m_rec_request);

    return g_tsStatus.get();
}

mfxStatus tsVideoENCPAK::Reset()
{
    //return Reset(m_session, m_pPar);
    TRACE_FUNC2(MFXVideoENC_Reset, m_session, enc.m_pPar);
    g_tsStatus.check( MFXVideoENC_Reset(m_session, enc.m_pPar) );

    TRACE_FUNC2(MFXVideoPAK_Reset, m_session, pak.m_pPar);
    g_tsStatus.check( MFXVideoPAK_Reset(m_session, pak.m_pPar) );

    //m_frames_buffered = 0;

    return g_tsStatus.get();
}

mfxStatus tsVideoENCPAK::AllocBitstream(mfxU32 size)
{
    if(!size)
    {
        if(enc.m_par.mfx.CodecId == MFX_CODEC_JPEG)
        {
            size = TS_MAX((enc.m_par.mfx.FrameInfo.Width*enc.m_par.mfx.FrameInfo.Height), 1000000);
        }
        else
        {
            if(!enc.m_par.mfx.BufferSizeInKB)
            {
                GetVideoParam();TS_CHECK_MFX;
            }
            size = enc.m_par.mfx.BufferSizeInKB * TS_MAX(enc.m_par.mfx.BRCParamMultiplier, 1) * 1000 * TS_MAX(enc.m_par.AsyncDepth, 1);
        }
    }

    g_tsLog << "ALLOC BITSTREAM OF SIZE " << size << "\n";

//    mfxMemId mid = 0;
//    TRACE_FUNC4((*m_buffer_allocator.Alloc), &m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid);
//    g_tsStatus.check((*m_buffer_allocator.Alloc)(&m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid));
//    TRACE_FUNC3((*m_buffer_allocator.Lock), &m_buffer_allocator, mid, &m_bitstream.Data);
//    g_tsStatus.check((*m_buffer_allocator.Lock)(&m_buffer_allocator, mid, &m_bitstream.Data));

    m_bitstream.Data = new mfxU8[size];
    m_bitstream.MaxLength = size;

    return g_tsStatus.get();
}

mfxStatus tsVideoENCPAK::GetVideoParam()
{
    if(m_default && !m_initialized)
    {
        Init();TS_CHECK_MFX;
    }
    //return GetVideoParam(m_session, m_pPar);
    TRACE_FUNC2(MFXVideoENC_GetVideoParam, m_session, enc.m_pPar);
    g_tsStatus.check( MFXVideoENC_GetVideoParam(m_session, enc.m_pPar) );
    TS_TRACE(enc.m_pPar);

    TRACE_FUNC2(MFXVideoPAK_GetVideoParam, m_session, pak.m_pPar);
    g_tsStatus.check( MFXVideoPAK_GetVideoParam(m_session, pak.m_pPar) );
    TS_TRACE(pak.m_pPar);

    return g_tsStatus.get();
}

mfxStatus tsVideoENCPAK::PrepareInitBuffers()
{
    enc.initbuf.clear();
    pak.initbuf.clear();

    PrepareParamBuf(enc.fpar, enc.m_par, enc.Func);
    enc.initbuf.push_back(&enc.fpar->Header);
    PrepareParamBuf(pak.fpar, pak.m_par, pak.Func);
    pak.initbuf.push_back(&pak.fpar->Header);

    PrepareSPSBuf(enc.fsps, enc.m_par);
    enc.initbuf.push_back(&enc.fsps->Header);
    PrepareSPSBuf(pak.fsps, pak.m_par);
    pak.initbuf.push_back(&pak.fsps->Header);

    // single pps for interlace too
    PreparePPSBuf(enc.fpps, enc.m_par);
    enc.initbuf.push_back(&enc.fpps->Header);
    PreparePPSBuf(pak.fpps, pak.m_par);
    pak.initbuf.push_back(&pak.fpps->Header);

    return MFX_ERR_NONE;
}

mfxStatus tsVideoENCPAK::PrepareDpbBuffers (bool secondField)
{
    const mfxU32 curField = secondField; // to avoid confusing
    std::vector<mfxFrameSurface1*> _refs = refs; // make local copy

    mfxExtFeiPPS::mfxExtFeiPpsDPB *pd = enc.fpps[curField].DpbBefore;

    for (int pass = 0; pass<2; pass++) { // before and after

        for (mfxU32 r=0, d=0; d < 16; r++, d++) {
            if (r >= _refs.size()) {
                pd[d].Index = 0xffff;
                continue;
            }
            mfxFrameSurface1* surf = _refs[r];
            std::vector<mfxFrameSurface1*>::iterator rslt;
            rslt = std::find(recSet.begin(), recSet.end(), surf);

            if (rslt == _refs.end()) {
                continue; // must be impossible
            } else {
                mfxU16 idx = static_cast<mfxU16>(std::distance(recSet.begin(), rslt));
                mfxU16 refType = surf->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MFX_PICTYPE_FRAME : (MFX_PICTYPE_TOPFIELD | MFX_PICTYPE_BOTTOMFIELD);
                pd[d].Index = idx;
                pd[d].PicType = refType;
                pd[d].FrameNumWrap = surf->Data.FrameOrder; // simplified
                pd[d].LongTermFrameIdx = 0xffff; // unused

                // change to field if interlace and current is its 2nd field
                if (refType != MFX_PICTYPE_FRAME && surf->Data.FrameOrder == m_PAKInput->InSurface->Data.FrameOrder) {
                    pd[d].PicType = surf->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD;
                }
            }
        }

        if (pass == 1) break;

        // update DPB - simplest way for a while
        mfxU16 type = enc.fpps[curField].FrameType;
        if (type & MFX_FRAMETYPE_REF) {
            if (type & MFX_FRAMETYPE_IDR)
                _refs.clear();
            else if (_refs.size() == enc.m_pPar->mfx.NumRefFrame)
                _refs.erase(_refs.begin());
            if (_refs.end() == std::find(_refs.begin(), _refs.end(), m_PAKOutput->OutSurface))
                _refs.push_back(m_PAKOutput->OutSurface);
        }
        pd = enc.fpps[curField].DpbAfter;
    }

    for (mfxU32 d=0; d < 16; d++) {
        pak.fpps[curField].DpbBefore[d] = enc.fpps[curField].DpbBefore[d];
        pak.fpps[curField].DpbAfter[d] = enc.fpps[curField].DpbAfter[d];
    }

    return MFX_ERR_NONE;
}

// is called for each field, but every call fills for both fields
mfxStatus tsVideoENCPAK::PrepareFrameBuffers (bool secondField)
{
    const mfxU32 curField = secondField; // to avoid confusing
    if (!secondField) {
        if (!m_enc_pool.PoolSize()) // shouldn't go into
        {
            if (m_pFrameAllocator && !m_enc_pool.GetAllocator())
            {
                m_enc_pool.SetAllocator(m_pFrameAllocator, true);
            }
            if (!m_pFrameAllocator && (m_enc_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
            {
                m_pFrameAllocator = m_enc_pool.GetAllocator();
                SetFrameAllocator();TS_CHECK_MFX;
            }
            if (m_pFrameAllocator && !m_rec_pool.GetAllocator())
            {
                m_rec_pool.SetAllocator(m_pFrameAllocator, true);
            }
            AllocSurfaces();TS_CHECK_MFX;
        }

        if(!m_initialized)
        {
            Init();TS_CHECK_MFX;
        }

        if(!m_bitstream.MaxLength)
        {
            AllocBitstream();TS_CHECK_MFX;
        }

        m_PAKInput->InSurface = m_ENCInput->InSurface = m_enc_pool.GetSurface(); TS_CHECK_MFX;
        m_ENCOutput->OutSurface = m_PAKOutput->OutSurface = m_rec_pool.GetSurface(); TS_CHECK_MFX;
        if (m_filler)
            m_PAKInput->InSurface = m_ENCInput->InSurface = m_filler->ProcessSurface(m_PAKInput->InSurface, m_pFrameAllocator);

        m_PAKOutput->Bs = m_pBitstream; // once for both fields?
    }

    recSet.resize(m_rec_pool.PoolSize());
    for (mfxU32 i = 0; i < m_rec_pool.PoolSize(); i++)
        recSet[i] = m_rec_pool.GetSurface(i);

    m_PAKInput->L0Surface = m_ENCInput->L0Surface = recSet.data();
    m_PAKInput->NumFrameL0 = m_ENCInput->NumFrameL0 = recSet.size();


    // refill all buffers for each field
    enc.inbuf.clear();
    enc.outbuf.clear();
    pak.inbuf.clear();
    pak.outbuf.clear();

    mfxU32 nfields = (enc.m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    for (mfxU32 field = 0; field < nfields; field++) {
        PrepareSliceBuf(enc.fslice, enc.m_par);
        PrepareCtrlBuf(enc.fctrl, enc.m_par);
        PreparePPSBuf(enc.fpps, enc.m_par);
        PrepareSliceBuf(pak.fslice, pak.m_par);
        PrepareCtrlBuf(pak.fctrl, pak.m_par);
        PreparePPSBuf(pak.fpps, pak.m_par);

        enc.inbuf.push_back(&enc.fpps[field].Header);
        enc.inbuf.push_back(&enc.fslice[field].Header);
        enc.inbuf.push_back(&enc.fctrl[field].Header);
        enc.outbuf.push_back(&mb[field].Header);
        enc.outbuf.push_back(&mv[field].Header);

        pak.inbuf.push_back(&pak.fpps[field].Header);
        pak.inbuf.push_back(&pak.fslice[field].Header);
        pak.inbuf.push_back(&mb[field].Header);
        pak.inbuf.push_back(&mv[field].Header);
    }


    mfxU16 frtype = 0, slicetype = 0;
    mfxI32 order = m_ENCInput->InSurface->Data.FrameOrder;
    mfxI32 goporder = order % enc.m_pPar->mfx.GopPicSize;
    if (goporder == 0 && !secondField) {
        frtype = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
        slicetype = 2; //FEI_SLICETYPE_I;
        if (enc.m_pPar->mfx.IdrInterval <= 1 || order / enc.m_pPar->mfx.GopPicSize % enc.m_pPar->mfx.IdrInterval == 0)
            frtype |= MFX_FRAMETYPE_IDR;
    }
    else if (goporder % enc.m_pPar->mfx.GopRefDist == 0) {
        frtype = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        slicetype = 0; //FEI_SLICETYPE_P;
    } else {
        frtype = MFX_FRAMETYPE_B;
        slicetype = 1; //FEI_SLICETYPE_B;
    }

    mfxFrameInfo &info = m_ENCInput->InSurface->Info;
    mfxU16 pictype = info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MFX_PICTYPE_FRAME :
            ((info.PicStruct == MFX_PICSTRUCT_FIELD_TFF && !secondField) ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);

    enc.fpps[curField].FrameType = frtype; // TODO for 2nd field
    enc.fpps[curField].PictureType = pictype;
    pak.fpps[curField].FrameType = frtype;
    pak.fpps[curField].PictureType = pictype;

    for (mfxI32 s=0; s<enc.fslice[curField].NumSlice; s++) {
        enc.fslice[curField].Slice[s].SliceType = slicetype;
        pak.fslice[curField].Slice[s].SliceType = slicetype;
        for (mfxU32 list=0; list<2; list++) {
            for (mfxU32 r=0; r<RefList[list].size(); r++) {
                // TODO find picture type for refs
                enc.fslice[curField].Slice[s].RefL0[r].Index       = RefList[list][RefList[list].size()-1 - r].dpb_idx; // simplest case, latest first
                enc.fslice[curField].Slice[s].RefL0[r].PictureType = RefList[list][RefList[list].size()-1 - r].type;
                pak.fslice[curField].Slice[s].RefL0[r].Index       = RefList[list][RefList[list].size()-1 - r].dpb_idx;
                pak.fslice[curField].Slice[s].RefL0[r].PictureType = RefList[list][RefList[list].size()-1 - r].type;
            }
        }
        enc.fslice[curField].Slice[s].PPSId             = enc.fpps[curField].PPSId;
        enc.fslice[curField].Slice[s].NumRefIdxL0Active = enc.fpps[curField].NumRefIdxL0Active = RefList[0].size();
        enc.fslice[curField].Slice[s].NumRefIdxL1Active = enc.fpps[curField].NumRefIdxL1Active = RefList[1].size();
        pak.fslice[curField].Slice[s].PPSId             = pak.fpps[curField].PPSId;
        pak.fslice[curField].Slice[s].NumRefIdxL0Active = pak.fpps[curField].NumRefIdxL0Active = RefList[0].size();
        pak.fslice[curField].Slice[s].NumRefIdxL1Active = pak.fpps[curField].NumRefIdxL1Active = RefList[1].size();
    }

    PrepareDpbBuffers(secondField);

    // HACK to pass checking in lib: after 1st == before 2nd, 2nd frameType
    if (enc.m_pPar->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
        if (!secondField) {
            for (mfxU32 d=0; d < 16; d++) {
                enc.fpps[1].DpbBefore[d] = enc.fpps[0].DpbAfter[d];
                pak.fpps[1].DpbBefore[d] = pak.fpps[0].DpbAfter[d];
            }
            enc.fpps[1].FrameType = enc.fpps[0].FrameType;
            if (enc.fpps[1].FrameType & MFX_FRAMETYPE_I)
                enc.fpps[1].FrameType = (enc.fpps[1].FrameType &~ (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR)) | MFX_FRAMETYPE_P;
            pak.fpps[1].FrameType = enc.fpps[1].FrameType;
        } else {
            for (mfxU32 d=0; d < 16; d++) {
                enc.fpps[0].DpbAfter[d] = enc.fpps[1].DpbBefore[d];
                pak.fpps[0].DpbAfter[d] = pak.fpps[1].DpbBefore[d];
            }
            enc.fpps[0].FrameType = enc.fpps[1].FrameType; // don't care, just to pass check
            pak.fpps[0].FrameType = enc.fpps[0].FrameType;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus tsVideoENCPAK::ProcessFrameAsync()
{

    m_ENCInput->ExtParam     = enc.inbuf.data();
    m_ENCInput->NumExtParam  = (mfxU16)enc.inbuf.size();
    m_ENCOutput->ExtParam    = enc.outbuf.data();
    m_ENCOutput->NumExtParam = (mfxU16)enc.outbuf.size();

    mfxStatus mfxRes = ProcessFrameAsync(m_session, m_ENCInput, m_ENCOutput, m_pSyncPoint);
    if (mfxRes != MFX_ERR_NONE)
        return mfxRes;

    mfxRes = SyncOperation(*m_pSyncPoint, 0);
    if (mfxRes != MFX_ERR_NONE)
        return mfxRes;

    m_PAKInput->ExtParam     = pak.inbuf.data();
    m_PAKInput->NumExtParam  = (mfxU16)pak.inbuf.size();
    m_PAKOutput->ExtParam    = pak.outbuf.data();
    m_PAKOutput->NumExtParam = (mfxU16)pak.outbuf.size();

    mfxRes = ProcessFrameAsync(m_session, m_PAKInput, m_PAKOutput, m_pSyncPoint);
    if (mfxRes != MFX_ERR_NONE)
        return mfxRes;

    mfxRes = SyncOperation(*m_pSyncPoint, m_bs_processor);
    if (mfxRes != MFX_ERR_NONE)
        return mfxRes;

    return MFX_ERR_NONE;
}

mfxStatus tsVideoENCPAK::ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp)
{
    TRACE_FUNC4(MFXVideoENC_ProcessFrameAsync, session, in, out, syncp);
    mfxStatus mfxRes = MFXVideoENC_ProcessFrameAsync(session, in, out, syncp);
    TS_TRACE(mfxRes);
    TS_TRACE(in);
    TS_TRACE(out);
    TS_TRACE(syncp);

    m_frames_buffered += (mfxRes >= 0);

    return g_tsStatus.m_status = mfxRes;
}

mfxStatus tsVideoENCPAK::ProcessFrameAsync(mfxSession session, mfxPAKInput *in, mfxPAKOutput *out, mfxSyncPoint *syncp)
{
    TRACE_FUNC4(MFXVideoPAK_ProcessFrameAsync, session, in, out, syncp);
    mfxStatus mfxRes = MFXVideoPAK_ProcessFrameAsync(session, in, out, syncp);
    TS_TRACE(mfxRes);
    TS_TRACE(in);
    TS_TRACE(out);
    TS_TRACE(syncp);

    //m_frames_buffered += (mfxRes >= 0);

    return g_tsStatus.m_status = mfxRes;
}


mfxStatus tsVideoENCPAK::SyncOperation()
{
    return SyncOperation(m_syncpoint, 0);
}

mfxStatus tsVideoENCPAK::SyncOperation(mfxSyncPoint syncp, tsBitstreamProcessor* bs_processor)
{
    mfxU32 nFrames = 1; // instead of m_frames_buffered;
    mfxStatus res = SyncOperation(m_session, syncp, MFX_INFINITE);

    // for PAK only
    if (m_default && bs_processor && g_tsStatus.get() == MFX_ERR_NONE)
    {
        g_tsStatus.check(bs_processor->ProcessBitstream(m_pBitstream ? *m_pBitstream : m_bitstream, nFrames));
        TS_CHECK_MFX;
    }

    return g_tsStatus.m_status = res;
}

mfxStatus tsVideoENCPAK::SyncOperation(mfxSession session,  mfxSyncPoint syncp, mfxU32 wait)
{
    m_frames_buffered = 0;
    return tsSession::SyncOperation(session, syncp, wait);
}

mfxStatus tsVideoENCPAK::AllocSurfaces()
{
    if(m_default && !m_enc_request.NumFrameMin)
    {
        QueryIOSurf(); TS_CHECK_MFX;
    }

    m_enc_pool.AllocSurfaces(m_enc_request); TS_CHECK_MFX;
    return m_rec_pool.AllocSurfaces(m_rec_request);
}


mfxStatus tsVideoENCPAK::EncodeFrame(bool secondField)
{
    mfxStatus sts = ProcessFrameAsync();
    if (sts != MFX_ERR_NONE)
        return sts;

    // update DPB - simplest way for a while
    mfxU16 type = ((mfxExtFeiPPS *)GetExtFeiBuffer(m_PAKInput->ExtParam, m_PAKInput->NumExtParam, MFX_EXTBUFF_FEI_PPS, secondField))->FrameType;
    if (type & MFX_FRAMETYPE_REF) {
        if (type & MFX_FRAMETYPE_IDR)
            refs.clear();
        else if (refs.size() == enc.m_pPar->mfx.NumRefFrame)
            refs.erase(refs.begin());
        if (refs.end() == std::find(refs.begin(), refs.end(), m_PAKOutput->OutSurface))
            refs.push_back(m_PAKOutput->OutSurface);
    }

    mfxFrameSurface1* input = m_PAKInput->InSurface;

    // map dpb to reconstruct pool, fill reflists
//    dpb_idx.clear();
    RefList[0].clear();
    RefList[1].clear();

    for (mfxU32 r=0; r<refs.size(); r++) {
        mfxFrameSurface1* surf = refs[r];
        std::vector<mfxFrameSurface1*>::iterator rslt;
        rslt = std::find(recSet.begin(), recSet.end(), surf);

        if (rslt == recSet.end()) {
            continue; // must be impossible
        } else {
            mfxU16 idx = static_cast<mfxU16>(std::distance(recSet.begin(), rslt));
//            dpb_idx.push_back(idx);
            // insert to reflists, TODO fix details
            bool isL1 = surf->Data.FrameOrder > input->Data.FrameOrder;
            mfxU16 refType = surf->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MFX_PICTYPE_FRAME :
                    (surf->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);
            RefListElem rle = {idx, refType};
            RefList[isL1].push_back(rle); // insert frame or first field
            // insert 2nd field if interlace and current is not its 2nd field
            if (refType != MFX_PICTYPE_FRAME && surf->Data.FrameOrder != input->Data.FrameOrder) {
                rle.type ^= MFX_PICTYPE_TOPFIELD ^ MFX_PICTYPE_BOTTOMFIELD; // change to opposite
                RefList[isL1].insert(RefList[isL1].end()-1, rle); // before last - finally same parity will be first
            }
        }
    }

    return sts;
}
