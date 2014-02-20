//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfxdefs.h"
#include "mfx_common_int.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "vm_event.h"
#include "vm_sys_info.h"
#include "mfx_ext_buffers.h"
#include <new>

#include "mfx_h265_encode.h"

#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "umc_structures.h"
#include "mfx_enc_common.h"

using namespace H265Enc;

//////////////////////////////////////////////////////////////////////////

#define CHECK_OPTION(input, output, corcnt) \
  if ( input != MFX_CODINGOPTION_OFF &&     \
      input != MFX_CODINGOPTION_ON  &&      \
      input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;      \
    (corcnt) ++;                            \
  } else output = input;

#define CHECK_EXTBUF_SIZE(ebuf, errcounter) if ((ebuf).Header.BufferSz != sizeof(ebuf)) {(errcounter) = (errcounter) + 1;}


namespace H265Enc {

static const mfxU16 H265_MAXREFDIST = 8;

typedef struct
{
    Ipp32s BufferSizeInKB;
    Ipp32s InitialDelayInKB;
    Ipp32s TargetKbps;
    Ipp32s MaxKbps;
} RcParams;

typedef struct
{
    mfxEncodeCtrl *ctrl;
    mfxFrameSurface1 *surface;
    mfxBitstream *bs;
} H265EncodeTaskInputParams;

mfxExtBuffer HEVC_HEADER = { MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC) };

mfxExtCodingOptionHEVC tab_tu[8] = {                    // CUS CUD 2TUS 2TUD  AnalyzeChroma         SignBitHiding          RDOQuant               SAO                   thrCU,TU,CUInter    5numCand1  5numCand2 WPP                       GPB                   AMP                   CmIntraThreshold TUSplitIntra CUSplit IntraAngModes EnableCm              BPyramid              FastPUDecision        HadamardMe TMVP
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 5,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 0,               1,           2,      1,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_OFF }, // tu default (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 6,  4, 5,2, 5,5,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_ON,    1, 1, 1,         8,8,4,4,4, 4,4,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON , 0,               1,           1,      1,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF, 2,         MFX_CODINGOPTION_ON  }, // tu 1
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 6,  3, 5,2, 5,5,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON , 0,               1,           2,      1,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_ON  }, // tu 2  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 6,  4, 5,2, 4,4,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON , 0,               1,           2,      1,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_ON  }, // tu 3  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 5,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 0,               1,           2,      1,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_OFF }, // tu 4
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 5,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 0,               3,           2,      2,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_OFF }, // tu 5  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  2, 5,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,    2, 2, 2,         6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 0,               3,           2,      2,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_OFF }, // tu 6  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  2, 5,2, 2,2,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,    3, 3, 3,         4,4,2,2,2, 2,2,1,1,1, MFX_CODINGOPTION_UNKNOWN, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 0,               3,           2,      2,            MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_OFF, 1,         MFX_CODINGOPTION_OFF }  // tu 7
};

Ipp8u tab_tuGopRefDist [8] = {4, 8, 8, 4, 4, 3, 2, 1};
Ipp8u tab_tuNumRefFrame[8] = {3, 4, 4, 3, 3, 2, 1, 1};

static const struct tab_hevcLevel {
    // General limits
    Ipp32s levelId;
    Ipp32s maxLumaPs;
    Ipp32s maxCPB[2]; // low/high tier, in 1000 bits
    Ipp32s maxSliceSegmentsPerPicture;
    Ipp32s maxTileRows;
    Ipp32s maxTileCols;
    // Main profiles limits
    Ipp64s maxLumaSr;
    Ipp32s maxBr[2]; // low/high tier, in 1000 bits
    Ipp32s minCr;
} tab_level[] = {
    {MFX_LEVEL_HEVC_1 ,    36864, {   350,      0},  16,  1,  1,     552960, {   128,      0}, 2},
    {MFX_LEVEL_HEVC_2 ,   122880, {  1500,      0},  16,  1,  1,    3686400, {  1500,      0}, 2},
    {MFX_LEVEL_HEVC_21,   245760, {  3000,      0},  20,  1,  1,    7372800, {  3000,      0}, 2},
    {MFX_LEVEL_HEVC_3 ,   552960, {  6000,      0},  30,  2,  2,   16588800, {  6000,      0}, 2},
    {MFX_LEVEL_HEVC_31,   983040, {  1000,      0},  40,  3,  3,   33177600, { 10000,      0}, 2},
    {MFX_LEVEL_HEVC_4 ,  2228224, { 12000,  30000},  75,  5,  5,   66846720, { 12000,  30000}, 4},
    {MFX_LEVEL_HEVC_41,  2228224, { 20000,  50000},  75,  5,  5,  133693440, { 20000,  50000}, 4},
    {MFX_LEVEL_HEVC_5 ,  8912896, { 25000, 100000}, 200, 11, 10,  267386880, { 25000, 100000}, 6},
    {MFX_LEVEL_HEVC_51,  8912896, { 40000, 160000}, 200, 11, 10,  534773760, { 40000, 160000}, 8},
    {MFX_LEVEL_HEVC_52,  8912896, { 60000, 240000}, 200, 11, 10, 1069547520, { 60000, 240000}, 8},
    {MFX_LEVEL_HEVC_6 , 35651584, { 60000, 240000}, 600, 22, 20, 1069547520, { 60000, 240000}, 8},
    {MFX_LEVEL_HEVC_61, 35651584, {120000, 480000}, 600, 22, 20, 2139095040, {120000, 480000}, 8},
    {MFX_LEVEL_HEVC_62, 35651584, {240000, 800000}, 600, 22, 20, 4278190080, {240000, 800000}, 6}
};

static const int    NUM_265_LEVELS = sizeof(tab_level) / sizeof(tab_level[0]);

//////////////////////////////////////////////////////////////////////////

static Ipp32s Check265Level(Ipp32s inLevelTier, const mfxVideoParam *parMfx)
{
    Ipp32s inLevel = inLevelTier &~ MFX_TIER_HEVC_HIGH;
    Ipp32s inTier = inLevelTier & MFX_TIER_HEVC_HIGH;
    Ipp32s level;
    for (level = 0; level < NUM_265_LEVELS; level++)
        if (tab_level[level].levelId == inLevel)
            break;

    if (level >= NUM_265_LEVELS) {
        if (inLevelTier != MFX_LEVEL_UNKNOWN)
            return MFX_LEVEL_UNKNOWN;
        inLevelTier = inLevel = MFX_LEVEL_UNKNOWN;
        inTier = 0;
        level = 0;
    }

    if (!parMfx) // just check for valid level value
        return inLevelTier;

    const mfxExtCodingOptionHEVC* opts  = (mfxExtCodingOptionHEVC*)GetExtBuffer(  parMfx->ExtParam,  parMfx->NumExtParam, MFX_EXTBUFF_HEVCENC );

    for( ; level < NUM_265_LEVELS; level++) {
        Ipp32s lumaPs = parMfx->mfx.FrameInfo.Width * parMfx->mfx.FrameInfo.Height;
        Ipp32s bitrate = parMfx->mfx.BRCParamMultiplier ? parMfx->mfx.BRCParamMultiplier * parMfx->mfx.TargetKbps : parMfx->mfx.TargetKbps;
        Ipp64s lumaSr = parMfx->mfx.FrameInfo.FrameRateExtD ? (Ipp64s)lumaPs * parMfx->mfx.FrameInfo.FrameRateExtN / parMfx->mfx.FrameInfo.FrameRateExtD : 0;
        if (lumaPs > tab_level[level].maxLumaPs)
            continue;
        if (parMfx->mfx.FrameInfo.Width * parMfx->mfx.FrameInfo.Width > 8 * tab_level[level].maxLumaPs)
            continue;
        if (parMfx->mfx.FrameInfo.Height * parMfx->mfx.FrameInfo.Height > 8 * tab_level[level].maxLumaPs)
            continue;
        if (parMfx->mfx.NumSlice > tab_level[level].maxSliceSegmentsPerPicture)
            continue;
        if (parMfx->mfx.FrameInfo.FrameRateExtD && lumaSr > tab_level[level].maxLumaSr)
            continue;
        if (bitrate > tab_level[level].maxBr[1]) // try high tier
            continue;
        if (bitrate && lumaSr * 3 / 2 * 1000 / bitrate < tab_level[level].minCr)
            continue; // it hardly can change the case

        // MaxDpbSIze not checked

        if (opts && opts->Log2MaxCUSize > 0 && tab_level[level].levelId >= MFX_LEVEL_HEVC_5 && opts->Log2MaxCUSize < 5)
            continue;

        Ipp32s tier = (bitrate > tab_level[level].maxBr[0]) ? MFX_TIER_HEVC_HIGH : MFX_TIER_HEVC_MAIN;

        if (inLevel == tab_level[level].levelId && tier <= inTier)
            return inLevelTier;
        if (inLevel <= tab_level[level].levelId)
            return tab_level[level].levelId | tier;
    }
    return MFX_LEVEL_UNKNOWN;
}

void SetCalcParams( RcParams* rc, const mfxVideoParam *parMfx )
{
    Ipp32s mult = IPP_MAX( parMfx->mfx.BRCParamMultiplier, 1);

    // not all fields are vlid for all rc modes
    rc->BufferSizeInKB = parMfx->mfx.BufferSizeInKB * mult;
    rc->TargetKbps = parMfx->mfx.TargetKbps * mult;
    rc->MaxKbps = parMfx->mfx.MaxKbps * mult;
    rc->InitialDelayInKB = parMfx->mfx.InitialDelayInKB * mult;
}

void GetCalcParams( mfxVideoParam *parMfx, const RcParams* rc, Ipp32s rcMode )
{
    Ipp32s maxVal = rc->BufferSizeInKB;
    if (rcMode == MFX_RATECONTROL_AVBR)
        maxVal = IPP_MAX(rc->TargetKbps, maxVal);
    else if (rcMode != MFX_RATECONTROL_CQP)
        maxVal = IPP_MAX( IPP_MAX( rc->InitialDelayInKB, rc->MaxKbps), maxVal);

    Ipp32s mult = (Ipp32u)(maxVal + 0xffff) >> 16;
    if (mult == 0)
        mult = 1;

    parMfx->mfx.BRCParamMultiplier = (mfxU16)mult;
    parMfx->mfx.BufferSizeInKB = (mfxU16)(rc->BufferSizeInKB / mult);
    if (rcMode != MFX_RATECONTROL_CQP) {
        parMfx->mfx.TargetKbps = (mfxU16)(rc->TargetKbps / mult);
        if (rcMode != MFX_RATECONTROL_AVBR) {
            parMfx->mfx.MaxKbps = (mfxU16)(rc->MaxKbps / mult);
            parMfx->mfx.InitialDelayInKB = (mfxU16)(rc->InitialDelayInKB / mult);
        }
    }
}


//inline Ipp32u h265enc_ConvertBitrate(mfxU16 TargetKbps)
//{
//    return (TargetKbps * 1000);
//}

// check for known ExtBuffers, returns error code. or -1 if found unknown
// zero mfxExtBuffer* are OK
mfxStatus CheckExtBuffers_H265enc(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{

    mfxU32 ID_list[] = { MFX_EXTBUFF_HEVCENC, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, MFX_EXTBUFF_DUMP };

    mfxU32 ID_found[sizeof(ID_list)/sizeof(ID_list[0])] = {0,};
    if (!ebuffers) return MFX_ERR_NONE;
    for(mfxU32 i=0; i<nbuffers; i++) {
        bool is_known = false;
        if (!ebuffers[i]) return MFX_ERR_NULL_PTR; //continue;
        for (mfxU32 j=0; j<sizeof(ID_list)/sizeof(ID_list[0]); j++)
            if (ebuffers[i]->BufferId == ID_list[j]) {
                if (ID_found[j])
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                is_known = true;
                ID_found[j] = 1; // to avoid duplicated
                break;
            }
        if (!is_known)
            return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

} // namespace

//////////////////////////////////////////////////////////////////////////

MFXVideoENCODEH265::MFXVideoENCODEH265(VideoCORE *core, mfxStatus *stat)
: VideoENCODE(),
  m_core(core),
  m_totalBits(0),
  m_encodedFrames(0),
  m_isInitialized(false),
  m_useSysOpaq(false),
  m_useVideoOpaq(false),
  m_isOpaque(false)
{
    ippStaticInit();
    *stat = MFX_ERR_NONE;
}

MFXVideoENCODEH265::~MFXVideoENCODEH265()
{
    Close();
}

mfxStatus MFXVideoENCODEH265::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    mfxStatus st = MFX_ERR_NONE;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    H265ENC_UNREFERENCED_PARAMETER(pInternalParams);
    MFX_CHECK(bs->MaxLength > (bs->DataOffset + bs->DataLength),MFX_ERR_UNDEFINED_BEHAVIOR);

    Ipp32s brcMult = IPP_MAX(1, m_mfxVideoParam.mfx.BRCParamMultiplier);
    if( (Ipp32s)(bs->MaxLength - bs->DataOffset - bs->DataLength) < m_mfxVideoParam.mfx.BufferSizeInKB * brcMult * 1000)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    if (surface)
    { // check frame parameters
        MFX_CHECK(surface->Info.ChromaFormat == m_mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Width  == m_mfxVideoParam.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height == m_mfxVideoParam.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);

        if (surface->Data.Y)
        {
            MFX_CHECK (surface->Data.UV, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK (surface->Data.Pitch < 0x8000 && surface->Data.Pitch!=0, MFX_ERR_UNDEFINED_BEHAVIOR);
        } else {
            MFX_CHECK (!surface->Data.UV, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    *reordered_surface = surface;

    if (m_mfxVideoParam.mfx.EncodedOrder) {
        return MFX_ERR_UNSUPPORTED;
    } else {
        if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) && ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)))
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        m_frameCountSync++;
        //if (surface)
        //    m_core->IncreaseReference(&(surface->Data));

        mfxU32 noahead = 1;

        if (!surface) {
            if (m_frameCountBufferedSync == 0) { // buffered frames to be encoded
                return MFX_ERR_MORE_DATA;
            }
            m_frameCountBufferedSync --;
        }
        else if (m_frameCountSync > noahead && m_frameCountBufferedSync <
            (mfxU32)m_mfxVideoParam.mfx.GopRefDist - noahead ) {
            m_frameCountBufferedSync++;
            return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
        }
    }

    return st;
}

mfxStatus MFXVideoENCODEH265::Init(mfxVideoParam* par_in)
{
    mfxStatus sts, stsQuery;
    MFX_CHECK(!m_isInitialized, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(par_in->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    sts = CheckVideoParamEncoders(par_in, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    sts = CheckExtBuffers_H265enc( par_in->ExtParam, par_in->NumExtParam );
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtDumpFiles* opts_Dump = (mfxExtDumpFiles*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_DUMP );

    const mfxVideoParam *par = par_in;

    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtBuffer *ptr_checked_ext[3] = {0};
    mfxU16 ext_counter = 0;
    memset(&m_mfxVideoParam,0,sizeof(mfxVideoParam));
    memset(&m_mfxHEVCOpts,0,sizeof(m_mfxHEVCOpts));
    memset(&m_mfxDumpFiles,0,sizeof(m_mfxDumpFiles));

    if (opts_hevc) {
        m_mfxHEVCOpts.Header.BufferId = MFX_EXTBUFF_HEVCENC;
        m_mfxHEVCOpts.Header.BufferSz = sizeof(m_mfxHEVCOpts);
        ptr_checked_ext[ext_counter++] = &m_mfxHEVCOpts.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (opts_Dump) {
        m_mfxDumpFiles.Header.BufferId = MFX_EXTBUFF_DUMP;
        m_mfxDumpFiles.Header.BufferSz = sizeof(m_mfxDumpFiles);
        ptr_checked_ext[ext_counter++] = &m_mfxDumpFiles.Header;
    }
    m_mfxVideoParam.ExtParam = ptr_checked_ext;
    m_mfxVideoParam.NumExtParam = ext_counter;

    stsQuery = Query(NULL, par_in, &m_mfxVideoParam); // [has to] copy all provided params

    // return status for Init differs in these cases
    if (stsQuery == MFX_ERR_UNSUPPORTED &&
        (  (par_in->mfx.EncodedOrder != m_mfxVideoParam.mfx.EncodedOrder) ||
           (par_in->mfx.NumSlice > 0 && m_mfxVideoParam.mfx.NumSlice == 0) ||
           (par_in->IOPattern != 0 && m_mfxVideoParam.IOPattern == 0) ||
           (par_in->mfx.RateControlMethod != 0 && m_mfxVideoParam.mfx.RateControlMethod == 0) ||
           (par_in->mfx.FrameInfo.PicStruct != 0 && m_mfxVideoParam.mfx.FrameInfo.PicStruct == 0) ||
           (par_in->mfx.FrameInfo.FrameRateExtN != 0 && m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN == 0) ||
           (par_in->mfx.FrameInfo.FrameRateExtD != 0 && m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD == 0) ||
           (par_in->mfx.FrameInfo.FourCC != 0 && m_mfxVideoParam.mfx.FrameInfo.FourCC == 0) ||
           (par_in->mfx.FrameInfo.ChromaFormat != 0 && m_mfxVideoParam.mfx.FrameInfo.ChromaFormat == 0) ) )
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (stsQuery != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        MFX_CHECK_STS(stsQuery);


    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (opaqAllocReq == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else {
            // check memory type in opaque allocation request
            if (!(opaqAllocReq->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && !(opaqAllocReq->In.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            if ((opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAllocReq->In.Type  & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            // use opaque surfaces. Need to allocate
            m_isOpaque = true;
        }
    }

    // return an error if requested opaque memory type isn't equal to native
    if (m_isOpaque && (opaqAllocReq->In.Type & MFX_MEMTYPE_FROM_ENCODE) && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //// to check if needed in hevc
    //m_allocator = new mfx_UMC_MemAllocator;
    //if (!m_allocator) return MFX_ERR_MEMORY_ALLOC;
    //
    //memset(&pParams, 0, sizeof(UMC::MemoryAllocatorParams));
    //m_allocator->InitMem(&pParams, m_core);

    if (m_isOpaque && opaqAllocReq->In.NumSurface < m_mfxVideoParam.mfx.GopRefDist)
        return MFX_ERR_INVALID_VIDEO_PARAM;

     // Allocate Opaque frames and frame for copy from video memory (if any)
    memset(&m_auxInput, 0, sizeof(m_auxInput));
    m_useAuxInput = false;
    m_useSysOpaq = false;
    m_useVideoOpaq = false;
    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_isOpaque) {
        bool bOpaqVideoMem = m_isOpaque && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
        bool bNeedAuxInput = (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || bOpaqVideoMem;
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        request.Info              = par->mfx.FrameInfo;

        // try to allocate opaque surfaces in video memory for another component in transcoding chain
        if (bOpaqVideoMem) {
            memset(&m_responseAlien, 0, sizeof(m_responseAlien));
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

            sts = m_core->AllocFrames(&request,
                                     &m_responseAlien,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);

            if (MFX_ERR_NONE != sts &&
                MFX_ERR_UNSUPPORTED != sts) // unsupported means that current Core couldn;t allocate the surfaces
                return sts;

            if (m_responseAlien.NumFrameActual < request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;

            if (sts != MFX_ERR_UNSUPPORTED)
                m_useVideoOpaq = true;
        }

        // allocate all we need in system memory
        memset(&m_response, 0, sizeof(m_response));
        if (bNeedAuxInput) {
            // allocate additional surface in system memory for FastCopy from video memory
            request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
            request.NumFrameMin       = 1;
            request.NumFrameSuggested = 1;
            sts = m_core->AllocFrames(&request, &m_response);
            MFX_CHECK_STS(sts);
        } else {
            // allocate opaque surfaces in system memory
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin       = opaqAllocReq->In.NumSurface;
            request.NumFrameSuggested = opaqAllocReq->In.NumSurface;
            sts = m_core->AllocFrames(&request,
                                     &m_response,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);
            MFX_CHECK_STS(sts);
        }

        if (m_response.NumFrameActual < request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        if (bNeedAuxInput) {
            m_useAuxInput = true;
            m_auxInput.Data.MemId = m_response.mids[0];
            m_auxInput.Info = request.Info;
        } else
            m_useSysOpaq = true;
    }



    mfxExtCodingOptionHEVC* opts_tu = &tab_tu[m_mfxVideoParam.mfx.TargetUsage];

    // check if size is aligned with CU depth
    int maxCUsize = m_mfxHEVCOpts.Log2MaxCUSize ? m_mfxHEVCOpts.Log2MaxCUSize : opts_tu->Log2MaxCUSize;
    int maxCUdepth = m_mfxHEVCOpts.MaxCUDepth ? m_mfxHEVCOpts.MaxCUDepth : opts_tu->MaxCUDepth;
    int minTUsize = m_mfxHEVCOpts.QuadtreeTULog2MinSize ? m_mfxHEVCOpts.QuadtreeTULog2MinSize : opts_tu->QuadtreeTULog2MinSize;

    int MinCUSize = 1 << (maxCUsize - maxCUdepth + 1);
    int tail = (m_mfxVideoParam.mfx.FrameInfo.Width | m_mfxVideoParam.mfx.FrameInfo.Height) & (MinCUSize-1);
    if (tail) { // size not aligned to minCU
        int logtail;
        for(logtail = 0; !(tail & (1<<logtail)); logtail++) ;
        if (logtail < 3) // min CU size is 8
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (logtail < minTUsize) { // size aligned to TU size
            if (m_mfxHEVCOpts.QuadtreeTULog2MinSize)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            minTUsize = m_mfxHEVCOpts.QuadtreeTULog2MinSize = (mfxU16)logtail;
        }
        if (m_mfxHEVCOpts.Log2MaxCUSize && m_mfxHEVCOpts.MaxCUDepth)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (!m_mfxHEVCOpts.MaxCUDepth)
            maxCUdepth = m_mfxHEVCOpts.MaxCUDepth = (mfxU16)(maxCUsize + 1 - logtail);
        else // !m_mfxHEVCOpts.Log2MaxCUSize
            maxCUsize = m_mfxHEVCOpts.Log2MaxCUSize = (mfxU16)(maxCUdepth - 1 + logtail);
    }

    // then fill not provided params with defaults or from targret usage
    if (!opts_hevc) {
        m_mfxHEVCOpts = tab_tu[m_mfxVideoParam.mfx.TargetUsage];
        m_mfxHEVCOpts.QuadtreeTULog2MinSize = (mfxU16)minTUsize;
        m_mfxHEVCOpts.MaxCUDepth = (mfxU16)maxCUdepth;
        m_mfxHEVCOpts.Log2MaxCUSize = (mfxU16)maxCUsize;
    } else { // complete unspecified params

        if(!m_mfxHEVCOpts.Log2MaxCUSize)
            m_mfxHEVCOpts.Log2MaxCUSize = opts_tu->Log2MaxCUSize;

        if(!m_mfxHEVCOpts.MaxCUDepth) // opts_in->MaxCUDepth <= opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.MaxCUDepth = IPP_MIN(opts_tu->MaxCUDepth, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if(!m_mfxHEVCOpts.QuadtreeTULog2MaxSize) // opts_in->QuadtreeTULog2MaxSize <= opts_out->Log2MaxCUSize
            m_mfxHEVCOpts.QuadtreeTULog2MaxSize = IPP_MIN(opts_tu->QuadtreeTULog2MaxSize, m_mfxHEVCOpts.Log2MaxCUSize);

        if(!m_mfxHEVCOpts.QuadtreeTULog2MinSize) // opts_in->QuadtreeTULog2MinSize <= opts_out->QuadtreeTULog2MaxSize
            m_mfxHEVCOpts.QuadtreeTULog2MinSize = IPP_MIN(opts_tu->QuadtreeTULog2MinSize, m_mfxHEVCOpts.QuadtreeTULog2MaxSize);

        if(!m_mfxHEVCOpts.QuadtreeTUMaxDepthIntra) // opts_in->QuadtreeTUMaxDepthIntra > opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.QuadtreeTUMaxDepthIntra = IPP_MIN(opts_tu->QuadtreeTUMaxDepthIntra, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if(!m_mfxHEVCOpts.QuadtreeTUMaxDepthInter) // opts_in->QuadtreeTUMaxDepthIntra > opts_out->Log2MaxCUSize - 1
            m_mfxHEVCOpts.QuadtreeTUMaxDepthInter = IPP_MIN(opts_tu->QuadtreeTUMaxDepthInter, m_mfxHEVCOpts.Log2MaxCUSize - 1);

        if (m_mfxHEVCOpts.AnalyzeChroma == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.AnalyzeChroma = opts_tu->AnalyzeChroma;

        // doesn't work together now, no sense
        if (m_mfxHEVCOpts.RDOQuant == MFX_CODINGOPTION_ON)
           m_mfxHEVCOpts.SignBitHiding = MFX_CODINGOPTION_OFF;
        if (m_mfxHEVCOpts.SignBitHiding == MFX_CODINGOPTION_ON)
           m_mfxHEVCOpts.RDOQuant = MFX_CODINGOPTION_OFF;
        if (m_mfxHEVCOpts.RDOQuant == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.RDOQuant = opts_tu->RDOQuant;
        if (m_mfxHEVCOpts.SignBitHiding == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.SignBitHiding = opts_tu->SignBitHiding;

        if (m_mfxHEVCOpts.WPP == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.WPP = opts_tu->WPP;

        if (m_mfxHEVCOpts.GPB == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.GPB = opts_tu->GPB;

        if (m_mfxHEVCOpts.AMP == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.AMP = opts_tu->AMP;

        if (m_mfxHEVCOpts.SAO == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.SAO = opts_tu->SAO;

        if (m_mfxHEVCOpts.BPyramid == MFX_CODINGOPTION_UNKNOWN)
            m_mfxHEVCOpts.BPyramid = opts_tu->BPyramid;

        if (!m_mfxHEVCOpts.SplitThresholdStrengthCUIntra)
            m_mfxHEVCOpts.SplitThresholdStrengthCUIntra = opts_tu->SplitThresholdStrengthCUIntra;
        if (!m_mfxHEVCOpts.SplitThresholdStrengthTUIntra)
            m_mfxHEVCOpts.SplitThresholdStrengthTUIntra = opts_tu->SplitThresholdStrengthTUIntra;
        if (!m_mfxHEVCOpts.SplitThresholdStrengthCUInter)
            m_mfxHEVCOpts.SplitThresholdStrengthCUInter = opts_tu->SplitThresholdStrengthCUInter;

        // take from tu, but correct if contradict with provided neighbours
        int pos;
        mfxU16 *candopt[10], candtu[10], *pleft[10], *plast;
        candopt[0] = &m_mfxHEVCOpts.IntraNumCand1_2;  candtu[0] = opts_tu->IntraNumCand1_2;
        candopt[1] = &m_mfxHEVCOpts.IntraNumCand1_3;  candtu[1] = opts_tu->IntraNumCand1_3;
        candopt[2] = &m_mfxHEVCOpts.IntraNumCand1_4;  candtu[2] = opts_tu->IntraNumCand1_4;
        candopt[3] = &m_mfxHEVCOpts.IntraNumCand1_5;  candtu[3] = opts_tu->IntraNumCand1_5;
        candopt[4] = &m_mfxHEVCOpts.IntraNumCand1_6;  candtu[4] = opts_tu->IntraNumCand1_6;
        candopt[5] = &m_mfxHEVCOpts.IntraNumCand2_2;  candtu[5] = opts_tu->IntraNumCand2_2;
        candopt[6] = &m_mfxHEVCOpts.IntraNumCand2_3;  candtu[6] = opts_tu->IntraNumCand2_3;
        candopt[7] = &m_mfxHEVCOpts.IntraNumCand2_4;  candtu[7] = opts_tu->IntraNumCand2_4;
        candopt[8] = &m_mfxHEVCOpts.IntraNumCand2_5;  candtu[8] = opts_tu->IntraNumCand2_5;
        candopt[9] = &m_mfxHEVCOpts.IntraNumCand2_6;  candtu[9] = opts_tu->IntraNumCand2_6;

        for(pos=0, plast=0; pos<10; pos++) {
            if (*candopt[pos]) plast = candopt[pos];
            pleft[pos] = plast;
        }
        for(pos=9, plast=0; pos>=0; pos--) {
            if (*candopt[pos]) plast = candopt[pos];
            else {
                mfxU16 val = candtu[pos];
                if(pleft[pos] && val>*pleft[pos]) val = *pleft[pos];
                if(plast      && val<*plast     ) val = *plast;
                *candopt[pos] = val;
            }
        }

        if (m_mfxHEVCOpts.CmIntraThreshold == 0)
            m_mfxHEVCOpts.CmIntraThreshold = opts_tu->CmIntraThreshold;
        if (m_mfxHEVCOpts.TUSplitIntra == 0)
            m_mfxHEVCOpts.TUSplitIntra = opts_tu->TUSplitIntra;
        if (m_mfxHEVCOpts.CUSplit == 0)
            m_mfxHEVCOpts.CUSplit = opts_tu->CUSplit;
        if (m_mfxHEVCOpts.IntraAngModes == 0)
            m_mfxHEVCOpts.IntraAngModes = opts_tu->IntraAngModes;
        if (m_mfxHEVCOpts.HadamardMe == 0)
            m_mfxHEVCOpts.HadamardMe = opts_tu->HadamardMe;
        if (m_mfxHEVCOpts.TMVP == 0)
            m_mfxHEVCOpts.TMVP = opts_tu->TMVP;
        if (m_mfxHEVCOpts.EnableCm == 0)
            m_mfxHEVCOpts.EnableCm = opts_tu->EnableCm;
        if (m_mfxHEVCOpts.FastPUDecision == 0)
            m_mfxHEVCOpts.FastPUDecision = opts_tu->FastPUDecision;
    }

    // uncomment here if sign bit hiding doesn't work properly
    //m_mfxHEVCOpts.SignBitHiding = MFX_CODINGOPTION_OFF;

    // check FrameInfo

    //FourCC to check on encode frame
    m_mfxVideoParam.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12; // to return on GetVideoParam

    // to be provided:
    if (!m_mfxVideoParam.mfx.FrameInfo.Width || !m_mfxVideoParam.mfx.FrameInfo.Height ||
        !m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN || !m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // Crops, AspectRatio - ignore

    if (!m_mfxVideoParam.mfx.FrameInfo.PicStruct) m_mfxVideoParam.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_mfxVideoParam.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    // CodecId - only HEVC;  CodecProfile CodecLevel - in the end
    // NumThread - set inside, >1 only if WPP
//    mfxU32 asyncDepth = par->AsyncDepth ? par->AsyncDepth : m_core->GetAutoAsyncDepth();
//    m_mfxVideoParam.mfx.NumThread = (mfxU16)asyncDepth;
//    if (MFX_PLATFORM_SOFTWARE != MFX_Utility::GetPlatform(m_core, par_in))
//        m_mfxVideoParam.mfx.NumThread = 1;
    m_mfxVideoParam.mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num(); // force 1 thread here

/*#if defined (AS_HEVCE_PLUGIN)
    m_mfxVideoParam.mfx.NumThread += 1;
#endif*/

    // TargetUsage - nothing to do

    // can depend on target usage
    if (!m_mfxVideoParam.mfx.GopRefDist) {
        m_mfxVideoParam.mfx.GopRefDist = tab_tuGopRefDist[m_mfxVideoParam.mfx.TargetUsage];
    }
    if (!m_mfxVideoParam.mfx.GopPicSize) {
        m_mfxVideoParam.mfx.GopPicSize = 60 * (mfxU16) (( m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN + m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD - 1 ) / m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD);
        if (m_mfxVideoParam.mfx.GopRefDist>1)
            m_mfxVideoParam.mfx.GopPicSize = (mfxU16) ((m_mfxVideoParam.mfx.GopPicSize + m_mfxVideoParam.mfx.GopRefDist - 1) / m_mfxVideoParam.mfx.GopRefDist * m_mfxVideoParam.mfx.GopRefDist);
    }
    //if (!m_mfxVideoParam.mfx.IdrInterval) m_mfxVideoParam.mfx.IdrInterval = m_mfxVideoParam.mfx.GopPicSize * 8;
    // GopOptFlag ignore

    if (m_mfxHEVCOpts.BPyramid == MFX_CODINGOPTION_ON) {
        Ipp32s GopRefDist = m_mfxVideoParam.mfx.GopRefDist;

        while (((GopRefDist & 1) == 0) && GopRefDist > 1)
            GopRefDist >>= 1;

        if (GopRefDist != 1 || (m_mfxVideoParam.mfx.GopPicSize % m_mfxVideoParam.mfx.GopRefDist)) {
            m_mfxHEVCOpts.BPyramid = MFX_CODINGOPTION_OFF; // not supported now
        }
    }

    // to be provided:
    if (!m_mfxVideoParam.mfx.RateControlMethod)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_mfxVideoParam.mfx.BufferSizeInKB)
        if ((m_mfxVideoParam.mfx.RateControlMethod == MFX_RATECONTROL_CBR || m_mfxVideoParam.mfx.RateControlMethod == MFX_RATECONTROL_VBR) &&
            m_mfxVideoParam.mfx.TargetKbps)
            m_mfxVideoParam.mfx.BufferSizeInKB = m_mfxVideoParam.mfx.TargetKbps * 2 / 8; // 2 seconds, like in AVC
        else
            m_mfxVideoParam.mfx.BufferSizeInKB = par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height * 3 / 2000 + 1; // uncompressed

    if (!m_mfxVideoParam.mfx.NumSlice) m_mfxVideoParam.mfx.NumSlice = 1;
    if (m_mfxHEVCOpts.SAO == MFX_CODINGOPTION_ON && m_mfxVideoParam.mfx.NumSlice > 1)
        m_mfxHEVCOpts.SAO = MFX_CODINGOPTION_OFF; // switch off SAO for now, because of inter-slice deblocking

    if (!m_mfxVideoParam.mfx.NumRefFrame) {
        if (m_mfxVideoParam.mfx.TargetUsage == MFX_TARGETUSAGE_1 && m_mfxVideoParam.mfx.GopRefDist == 1) {
            m_mfxVideoParam.mfx.NumRefFrame = 4; // Low_Delay
        } else {
            m_mfxVideoParam.mfx.NumRefFrame = tab_tuNumRefFrame[m_mfxVideoParam.mfx.TargetUsage];
        }
    }

    // ALL INTRA
    if (m_mfxVideoParam.mfx.GopPicSize == 1) {
        m_mfxVideoParam.mfx.GopRefDist = 1;
        m_mfxVideoParam.mfx.NumRefFrame = 1;
    }

    m_mfxVideoParam.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
    mfxI32 complevel = Check265Level(m_mfxVideoParam.mfx.CodecLevel, &m_mfxVideoParam);
    if (complevel != m_mfxVideoParam.mfx.CodecLevel) {
        if (m_mfxVideoParam.mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
            stsQuery = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        m_mfxVideoParam.mfx.CodecLevel = (mfxU16)complevel;
    }

    // what is still needed?
    //m_mfxVideoParam.mfx = par->mfx;
    //m_mfxVideoParam.SetCalcParams(&m_mfxVideoParam);
    //m_mfxVideoParam.calcParam = par->calcParam;
    m_mfxVideoParam.IOPattern = par->IOPattern;
    m_mfxVideoParam.Protected = 0;
    m_mfxVideoParam.AsyncDepth = par->AsyncDepth;

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_frameCountBufferedSync = 0;
    m_frameCountBuffered = 0;

    vm_event_set_invalid(&m_taskParams.parallel_region_done);
    if (VM_OK != vm_event_init(&m_taskParams.parallel_region_done, 0, 0))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    vm_mutex_set_invalid(&m_taskParams.parallel_region_end_lock);
    if (VM_OK != vm_mutex_init(&m_taskParams.parallel_region_end_lock)) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;
    m_taskParams.parallel_execution_in_progress = false;

    m_enc = new H265Encoder();
    MFX_CHECK_STS_ALLOC(m_enc);

    m_enc->mfx_video_encode_h265_ptr = this;
    sts =  m_enc->Init(&m_mfxVideoParam, &m_mfxHEVCOpts);
    MFX_CHECK_STS(sts);
    m_enc->m_recon_dump_file_name = m_mfxDumpFiles.ReconFilename[0] ? m_mfxDumpFiles.ReconFilename : NULL;

    m_isInitialized = true;

    if (MFX_ERR_NONE == stsQuery)
        return (m_core->GetPlatformType() == MFX_PLATFORM_SOFTWARE)?MFX_ERR_NONE:MFX_WRN_PARTIAL_ACCELERATION;

    return stsQuery;
}

mfxStatus MFXVideoENCODEH265::Close()
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    vm_event_destroy(&m_taskParams.parallel_region_done);
    vm_mutex_destroy(&m_taskParams.parallel_region_end_lock);

    if (m_enc) {
        m_enc->Close();
        delete m_enc;
        m_enc = NULL;
    }

    if (m_useAuxInput || m_useSysOpaq) {
        //st = m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        m_core->FreeFrames(&m_response);
        m_response.NumFrameActual = 0;
        m_useAuxInput = false;
        m_useSysOpaq = false;
    }

    if (m_useVideoOpaq) {
        m_core->FreeFrames(&m_responseAlien);
        m_responseAlien.NumFrameActual = 0;
        m_useVideoOpaq = false;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Reset(mfxVideoParam *par_in)
{
    mfxStatus sts, stsQuery;

    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    sts = CheckExtBuffers_H265enc( par_in->ExtParam, par_in->NumExtParam );
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtDumpFiles* opts_Dump = (mfxExtDumpFiles*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_DUMP );

    mfxVideoParam parNew = *par_in;
    mfxVideoParam& parOld = m_mfxVideoParam;

        //set unspecified parameters
    if (!parNew.AsyncDepth)             parNew.AsyncDepth           = parOld.AsyncDepth;
    //if (!parNew.IOPattern)              parNew.IOPattern            = parOld.IOPattern;
    if (!parNew.mfx.FrameInfo.FourCC)   parNew.mfx.FrameInfo.FourCC = parOld.mfx.FrameInfo.FourCC;
    if (!parNew.mfx.FrameInfo.FrameRateExtN) parNew.mfx.FrameInfo.FrameRateExtN = parOld.mfx.FrameInfo.FrameRateExtN;
    if (!parNew.mfx.FrameInfo.FrameRateExtD) parNew.mfx.FrameInfo.FrameRateExtD = parOld.mfx.FrameInfo.FrameRateExtD;
    if (!parNew.mfx.FrameInfo.AspectRatioW)  parNew.mfx.FrameInfo.AspectRatioW  = parOld.mfx.FrameInfo.AspectRatioW;
    if (!parNew.mfx.FrameInfo.AspectRatioH)  parNew.mfx.FrameInfo.AspectRatioH  = parOld.mfx.FrameInfo.AspectRatioH;
    if (!parNew.mfx.FrameInfo.PicStruct)     parNew.mfx.FrameInfo.PicStruct     = parOld.mfx.FrameInfo.PicStruct;
    //if (!parNew.mfx.FrameInfo.ChromaFormat) parNew.mfx.FrameInfo.ChromaFormat = parOld.mfx.FrameInfo.ChromaFormat;
    if (!parNew.mfx.CodecProfile)       parNew.mfx.CodecProfile      = parOld.mfx.CodecProfile;
    if (!parNew.mfx.CodecLevel)         parNew.mfx.CodecLevel        = parOld.mfx.CodecLevel;
    if (!parNew.mfx.NumThread)          parNew.mfx.NumThread         = parOld.mfx.NumThread;
    if (!parNew.mfx.TargetUsage)        parNew.mfx.TargetUsage       = parOld.mfx.TargetUsage;

    if (!parNew.mfx.FrameInfo.Width)    parNew.mfx.FrameInfo.Width  = parOld.mfx.FrameInfo.Width;
    if (!parNew.mfx.FrameInfo.Height)   parNew.mfx.FrameInfo.Height = parOld.mfx.FrameInfo.Height;
    //if (!parNew.mfx.FrameInfo.CropX)    parNew.mfx.FrameInfo.CropX  = parOld.mfx.FrameInfo.CropX;
    //if (!parNew.mfx.FrameInfo.CropY)    parNew.mfx.FrameInfo.CropY  = parOld.mfx.FrameInfo.CropY;
    //if (!parNew.mfx.FrameInfo.CropW)    parNew.mfx.FrameInfo.CropW  = parOld.mfx.FrameInfo.CropW;
    //if (!parNew.mfx.FrameInfo.CropH)    parNew.mfx.FrameInfo.CropH  = parOld.mfx.FrameInfo.CropH;
    if (!parNew.mfx.GopRefDist)         parNew.mfx.GopRefDist        = parOld.mfx.GopRefDist;
    if (!parNew.mfx.GopOptFlag)         parNew.mfx.GopOptFlag        = parOld.mfx.GopOptFlag;
    if (!parNew.mfx.RateControlMethod)  parNew.mfx.RateControlMethod = parOld.mfx.RateControlMethod;
    if (!parNew.mfx.NumSlice)           parNew.mfx.NumSlice          = parOld.mfx.NumSlice;
    if (!parNew.mfx.NumRefFrame)        parNew.mfx.NumRefFrame       = parOld.mfx.NumRefFrame;

    /*if(    parNew.mfx.CodecProfile != MFX_PROFILE_AVC_STEREO_HIGH
        && parNew.mfx.CodecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH)*/{
        mfxU16 old_multiplier = IPP_MAX(parOld.mfx.BRCParamMultiplier, 1);
        mfxU16 new_multiplier = IPP_MAX(parNew.mfx.BRCParamMultiplier, 1);

        if (old_multiplier > new_multiplier) {
            parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parNew.mfx.BufferSizeInKB*new_multiplier/old_multiplier);
            if (parNew.mfx.RateControlMethod == MFX_RATECONTROL_CBR || parNew.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parNew.mfx.InitialDelayInKB*new_multiplier/old_multiplier);
                parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parNew.mfx.TargetKbps      *new_multiplier/old_multiplier);
                parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parNew.mfx.MaxKbps         *new_multiplier/old_multiplier);
            }
            new_multiplier = old_multiplier;
            parNew.mfx.BRCParamMultiplier = new_multiplier;
        }
        if (!parNew.mfx.BufferSizeInKB) parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parOld.mfx.BufferSizeInKB*old_multiplier/new_multiplier);
        if (parNew.mfx.RateControlMethod == parOld.mfx.RateControlMethod) {
            if (parNew.mfx.RateControlMethod > MFX_RATECONTROL_VBR)
                old_multiplier = new_multiplier = 1;
            if (!parNew.mfx.InitialDelayInKB) parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parOld.mfx.InitialDelayInKB*old_multiplier/new_multiplier);
            if (!parNew.mfx.TargetKbps)       parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parOld.mfx.TargetKbps      *old_multiplier/new_multiplier);
            if (!parNew.mfx.MaxKbps)          parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parOld.mfx.MaxKbps         *old_multiplier/new_multiplier);
        }
    }

    mfxExtCodingOptionHEVC optsNew;
    if (opts_hevc) {
        mfxExtCodingOptionHEVC& optsOld = m_mfxHEVCOpts;
        optsNew = *opts_hevc;
        if (!optsNew.Log2MaxCUSize                ) optsNew.Log2MaxCUSize                 = optsOld.Log2MaxCUSize;
        if (!optsNew.MaxCUDepth                   ) optsNew.MaxCUDepth                    = optsOld.MaxCUDepth;
        if (!optsNew.QuadtreeTULog2MaxSize        ) optsNew.QuadtreeTULog2MaxSize         = optsOld.QuadtreeTULog2MaxSize        ;
        if (!optsNew.QuadtreeTULog2MinSize        ) optsNew.QuadtreeTULog2MinSize         = optsOld.QuadtreeTULog2MinSize        ;
        if (!optsNew.QuadtreeTUMaxDepthIntra      ) optsNew.QuadtreeTUMaxDepthIntra       = optsOld.QuadtreeTUMaxDepthIntra      ;
        if (!optsNew.QuadtreeTUMaxDepthInter      ) optsNew.QuadtreeTUMaxDepthInter       = optsOld.QuadtreeTUMaxDepthInter      ;
        if (!optsNew.AnalyzeChroma                ) optsNew.AnalyzeChroma                 = optsOld.AnalyzeChroma                ;
        if (!optsNew.SignBitHiding                ) optsNew.SignBitHiding                 = optsOld.SignBitHiding                ;
        if (!optsNew.RDOQuant                     ) optsNew.RDOQuant                      = optsOld.RDOQuant                     ;
        if (!optsNew.SAO                          ) optsNew.SAO                           = optsOld.SAO                          ;
        if (!optsNew.SplitThresholdStrengthCUIntra) optsNew.SplitThresholdStrengthCUIntra = optsOld.SplitThresholdStrengthCUIntra;
        if (!optsNew.SplitThresholdStrengthTUIntra) optsNew.SplitThresholdStrengthTUIntra = optsOld.SplitThresholdStrengthTUIntra;
        if (!optsNew.SplitThresholdStrengthCUInter) optsNew.SplitThresholdStrengthCUInter = optsOld.SplitThresholdStrengthCUInter;
        if (!optsNew.IntraNumCand1_2              ) optsNew.IntraNumCand1_2               = optsOld.IntraNumCand1_2              ;
        if (!optsNew.IntraNumCand1_3              ) optsNew.IntraNumCand1_3               = optsOld.IntraNumCand1_3              ;
        if (!optsNew.IntraNumCand1_4              ) optsNew.IntraNumCand1_4               = optsOld.IntraNumCand1_4              ;
        if (!optsNew.IntraNumCand1_5              ) optsNew.IntraNumCand1_5               = optsOld.IntraNumCand1_5              ;
        if (!optsNew.IntraNumCand1_6              ) optsNew.IntraNumCand1_6               = optsOld.IntraNumCand1_6              ;
        if (!optsNew.IntraNumCand2_2              ) optsNew.IntraNumCand2_2               = optsOld.IntraNumCand2_2              ;
        if (!optsNew.IntraNumCand2_3              ) optsNew.IntraNumCand2_3               = optsOld.IntraNumCand2_3              ;
        if (!optsNew.IntraNumCand2_4              ) optsNew.IntraNumCand2_4               = optsOld.IntraNumCand2_4              ;
        if (!optsNew.IntraNumCand2_5              ) optsNew.IntraNumCand2_5               = optsOld.IntraNumCand2_5              ;
        if (!optsNew.IntraNumCand2_6              ) optsNew.IntraNumCand2_6               = optsOld.IntraNumCand2_6              ;
        if (!optsNew.WPP                          ) optsNew.WPP                           = optsOld.WPP                          ;
        if (!optsNew.GPB                          ) optsNew.GPB                           = optsOld.GPB                          ;
        if (!optsNew.AMP                          ) optsNew.AMP                           = optsOld.AMP                          ;
        if (!optsNew.BPyramid                     ) optsNew.BPyramid                      = optsOld.BPyramid                          ;
    }

    if ((parNew.IOPattern & 0xffc8) || (parNew.IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (parNew.IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;


    sts = CheckVideoParamEncoders(&parNew, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    mfxVideoParam checked_videoParam = parNew;
    mfxExtCodingOptionHEVC checked_codingOptionHEVC = m_mfxHEVCOpts;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtDumpFiles checked_DumpFiles = m_mfxDumpFiles;

    mfxExtBuffer *ptr_input_ext[3] = {0};
    mfxExtBuffer *ptr_checked_ext[3] = {0};
    mfxU16 ext_counter = 0;

    if (opts_hevc) {
        ptr_input_ext  [ext_counter  ] = &optsNew.Header;
        ptr_checked_ext[ext_counter++] = &checked_codingOptionHEVC.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_input_ext  [ext_counter  ] = &opaqAllocReq->Header;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (opts_Dump) {
        ptr_input_ext  [ext_counter  ] = &opts_Dump->Header;
        ptr_checked_ext[ext_counter++] = &checked_DumpFiles.Header;
    }
    parNew.ExtParam = ptr_input_ext;
    parNew.NumExtParam = ext_counter;
    checked_videoParam.ExtParam = ptr_checked_ext;
    checked_videoParam.NumExtParam = ext_counter;

    stsQuery = Query(NULL, &parNew, &checked_videoParam); // [has to] copy all provided params

    if (stsQuery != MFX_ERR_NONE && stsQuery != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
        if (stsQuery == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return stsQuery;
    }

    // check that new params don't require allocation of additional memory
    if (checked_videoParam.mfx.FrameInfo.Width > m_mfxVideoParam.mfx.FrameInfo.Width ||
        checked_videoParam.mfx.FrameInfo.Height > m_mfxVideoParam.mfx.FrameInfo.Height ||
        checked_videoParam.mfx.GopRefDist <  m_mfxVideoParam.mfx.GopRefDist ||
        checked_videoParam.mfx.NumSlice != m_mfxVideoParam.mfx.NumSlice ||
        checked_videoParam.mfx.NumRefFrame != m_mfxVideoParam.mfx.NumRefFrame ||
        checked_videoParam.AsyncDepth != m_mfxVideoParam.AsyncDepth ||
        checked_videoParam.IOPattern != m_mfxVideoParam.IOPattern ||
        (checked_videoParam.mfx.CodecProfile & 0xFF) != (m_mfxVideoParam.mfx.CodecProfile & 0xFF)  )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // Now use simple reset
    Close();
    m_isInitialized = false;
    sts = Init(&checked_videoParam);

    // Not finished implementation

    //m_frameCountSync = 0;
    //m_frameCount = 0;
    //m_frameCountBufferedSync = 0;
    //m_frameCountBuffered = 0;

//    sts = m_enc->Reset();
//    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus MFXVideoENCODEH265::Query(VideoCORE *core, mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    mfxStatus st;
    MFX_CHECK_NULL_PTR1(par_out)

    //First check for zero input
    if( par_in == NULL ){ //Set ones for filed that can be configured
        mfxVideoParam *out = par_out;
        memset( &out->mfx, 0, sizeof( mfxInfoMFX ));
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.CodecId = MFX_CODEC_HEVC; // restore cleared mandatory
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.IdrInterval = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.InitialDelayInKB = 1;
        out->mfx.BufferSizeInKB = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.MaxKbps = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumRefFrame = 1;
        out->mfx.EncodedOrder = 0;
        out->AsyncDepth = 1;
        out->IOPattern = 1;
        out->Protected = 0;

        //Extended coding options
        st = CheckExtBuffers_H265enc( out->ExtParam, out->NumExtParam );
        MFX_CHECK_STS(st);

        mfxExtCodingOptionHEVC* optsHEVC = (mfxExtCodingOptionHEVC*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_HEVCENC );
        if (optsHEVC != 0) {
            mfxExtBuffer HeaderCopy = optsHEVC->Header;
            memset( optsHEVC, 0, sizeof( mfxExtCodingOptionHEVC ));
            optsHEVC->Header = HeaderCopy;
            optsHEVC->Log2MaxCUSize = 1;
            optsHEVC->MaxCUDepth = 1;
            optsHEVC->QuadtreeTULog2MaxSize = 1;
            optsHEVC->QuadtreeTULog2MinSize = 1;
            optsHEVC->QuadtreeTUMaxDepthIntra = 1;
            optsHEVC->QuadtreeTUMaxDepthInter = 1;
            optsHEVC->AnalyzeChroma = 1;              /* tri-state option */
            optsHEVC->SignBitHiding = 1;
            optsHEVC->RDOQuant = 1;
            optsHEVC->SAO      = 1;
            optsHEVC->SplitThresholdStrengthCUIntra = 1;
            optsHEVC->SplitThresholdStrengthTUIntra = 1;
            optsHEVC->SplitThresholdStrengthCUInter = 1;
            optsHEVC->IntraNumCand1_2 = 1;
            optsHEVC->IntraNumCand1_3 = 1;
            optsHEVC->IntraNumCand1_4 = 1;
            optsHEVC->IntraNumCand1_5 = 1;
            optsHEVC->IntraNumCand1_6 = 1;
            optsHEVC->IntraNumCand2_2 = 1;
            optsHEVC->IntraNumCand2_3 = 1;
            optsHEVC->IntraNumCand2_4 = 1;
            optsHEVC->IntraNumCand2_5 = 1;
            optsHEVC->IntraNumCand2_6 = 1;
            optsHEVC->WPP = 1;
            optsHEVC->GPB = 1;
            optsHEVC->AMP = 1;
            optsHEVC->BPyramid = 1;
        }

        mfxExtDumpFiles* optsDump = (mfxExtDumpFiles*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_DUMP );
        if (optsDump != 0) {
            mfxExtBuffer HeaderCopy = optsDump->Header;
            memset( optsDump, 0, sizeof( mfxExtDumpFiles ));
            optsDump->Header = HeaderCopy;
            optsDump->ReconFilename[0] = 1;
        }
        // ignore all reserved
    } else { //Check options for correctness
        const mfxVideoParam * const in = par_in;
        mfxVideoParam * const out = par_out;

        if ( in->mfx.CodecId != MFX_CODEC_HEVC)
            return MFX_ERR_UNSUPPORTED;

        st = CheckExtBuffers_H265enc( in->ExtParam, in->NumExtParam );
        MFX_CHECK_STS(st);
        st = CheckExtBuffers_H265enc( out->ExtParam, out->NumExtParam );
        MFX_CHECK_STS(st);

        const mfxExtCodingOptionHEVC* opts_in = (mfxExtCodingOptionHEVC*)GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_HEVCENC );
        mfxExtCodingOptionHEVC*      opts_out = (mfxExtCodingOptionHEVC*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_HEVCENC );
        const mfxExtDumpFiles*    optsDump_in = (mfxExtDumpFiles*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_DUMP );
        mfxExtDumpFiles*         optsDump_out = (mfxExtDumpFiles*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_DUMP );

        if (opts_in           ) CHECK_EXTBUF_SIZE( *opts_in,            isInvalid)
        if (opts_out          ) CHECK_EXTBUF_SIZE( *opts_out,           isInvalid)
        if (optsDump_in       ) CHECK_EXTBUF_SIZE( *optsDump_in,        isInvalid)
        if (optsDump_out      ) CHECK_EXTBUF_SIZE( *optsDump_out,       isInvalid)
        if (isInvalid)
            return MFX_ERR_UNSUPPORTED;

        if ((opts_in==0) != (opts_out==0) ||
            (optsDump_in==0) != (optsDump_out==0))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        //if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 && in->mfx.FrameInfo.ChromaFormat == 0) { // because 0 == monochrome
        //    out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        //    out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        //    isCorrected ++;
        //} else
        if ((in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12) != (in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)) {
            out->mfx.FrameInfo.FourCC = 0;
            out->mfx.FrameInfo.ChromaFormat = 0;
            isInvalid ++;
        } else {
            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        }

        if (in->Protected != 0)
            return MFX_ERR_UNSUPPORTED;
        out->Protected = 0;

        out->AsyncDepth = in->AsyncDepth;

        if ( (in->mfx.FrameInfo.Width & 15) || in->mfx.FrameInfo.Width > 3840 ) {
            out->mfx.FrameInfo.Width = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
        if ( (in->mfx.FrameInfo.Height & 15) || in->mfx.FrameInfo.Height > 2160 ) {
            out->mfx.FrameInfo.Height = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;

        // Check crops
        if (in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height && in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropH = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
        if (in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropW = 0;
            isInvalid ++;
        } else  out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
        if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropX = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
        if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropY = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        // Assume 420 checking horizontal crop to be even
        if ((in->mfx.FrameInfo.CropX & 1) && (in->mfx.FrameInfo.CropW & 1)) {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        } else if (in->mfx.FrameInfo.CropX & 1)
        {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 2;
            isCorrected ++;
        }
        else if (in->mfx.FrameInfo.CropW & 1) {
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        }

        // Assume 420 checking horizontal crop to be even
        if ((in->mfx.FrameInfo.CropY & 1) && (in->mfx.FrameInfo.CropH & 1)) {
            if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY + 1;
            if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 1;
            isCorrected ++;
        } else if (in->mfx.FrameInfo.CropY & 1)
        {
            if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY + 1;
            if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 2;
            isCorrected ++;
        }
        else if (in->mfx.FrameInfo.CropH & 1) {
            if (out->mfx.FrameInfo.CropH) // can't decrement zero CropH
                out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH - 1;
            isCorrected ++;
        }

        //Check for valid framerate
        if (!in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD ||
            in->mfx.FrameInfo.FrameRateExtN && !in->mfx.FrameInfo.FrameRateExtD ||
            in->mfx.FrameInfo.FrameRateExtD && ((mfxF64)in->mfx.FrameInfo.FrameRateExtN / in->mfx.FrameInfo.FrameRateExtD) > 120) {
            isInvalid ++;
            out->mfx.FrameInfo.FrameRateExtN = out->mfx.FrameInfo.FrameRateExtD = 0;
        } else {
            out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
            out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;
        }

        switch(in->IOPattern) {
            case 0:
            case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
            case MFX_IOPATTERN_IN_VIDEO_MEMORY:
            case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                out->IOPattern = in->IOPattern;
                break;
            default:
                if (in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
                } else if (in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
                } else {
                    isInvalid ++;
                    out->IOPattern = 0;
                }
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if (in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN ) {
            //out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            //isCorrected ++;
            out->mfx.FrameInfo.PicStruct = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;

        out->mfx.BRCParamMultiplier = in->mfx.BRCParamMultiplier;

        out->mfx.CodecId = MFX_CODEC_HEVC;
        if (in->mfx.CodecProfile != MFX_PROFILE_UNKNOWN && in->mfx.CodecProfile != MFX_PROFILE_HEVC_MAIN) {
            isCorrected ++;
            out->mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
        } else out->mfx.CodecProfile = in->mfx.CodecProfile;
        if (in->mfx.CodecLevel != MFX_LEVEL_UNKNOWN) {
            mfxI32 complevel = Check265Level(in->mfx.CodecLevel, in);
            if (complevel != in->mfx.CodecLevel) {
                if (complevel == MFX_LEVEL_UNKNOWN)
                    isInvalid ++;
                else
                    isCorrected ++;
            }
            out->mfx.CodecLevel = (mfxU16)complevel;
        }
        else out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;

        out->mfx.NumThread = in->mfx.NumThread;

        if (in->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED) {
            isCorrected ++;
            out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        }
        else out->mfx.TargetUsage = in->mfx.TargetUsage;

        out->mfx.GopPicSize = in->mfx.GopPicSize;

        if (in->mfx.GopRefDist > H265_MAXREFDIST) {
            out->mfx.GopRefDist = H265_MAXREFDIST;
            isCorrected ++;
        }
        else out->mfx.GopRefDist = in->mfx.GopRefDist;

        if (in->mfx.NumRefFrame > MAX_NUM_REF_IDX) {
            out->mfx.NumRefFrame = MAX_NUM_REF_IDX;
            isCorrected ++;
        }
        else out->mfx.NumRefFrame = in->mfx.NumRefFrame;

        if ((in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) != in->mfx.GopOptFlag) {
            out->mfx.GopOptFlag = 0;
            isInvalid ++;
        } else out->mfx.GopOptFlag = in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);

        out->mfx.IdrInterval = in->mfx.IdrInterval;

        // NumSlice will be checked again after Log2MaxCUSize
        out->mfx.NumSlice = in->mfx.NumSlice;
        if ( in->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Width /*&& opts_out && opts_out->Log2MaxCUSize*/)
        {
            mfxU16 rnd = (1 << 4) - 1, shft = 4;
            if (in->mfx.NumSlice > ((out->mfx.FrameInfo.Height+rnd)>>shft) * ((out->mfx.FrameInfo.Width+rnd)>>shft) )
            {
                out->mfx.NumSlice = 0;
                isInvalid ++;
            }
        }
        out->mfx.NumRefFrame = in->mfx.NumRefFrame;

        if (in->mfx.EncodedOrder != 0) {
            isInvalid ++;
        }
        out->mfx.EncodedOrder = 0;

        if (in->mfx.RateControlMethod != 0 &&
            in->mfx.RateControlMethod != MFX_RATECONTROL_CBR && in->mfx.RateControlMethod != MFX_RATECONTROL_VBR && in->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
            out->mfx.RateControlMethod = 0;
            isInvalid ++;
        } else out->mfx.RateControlMethod = in->mfx.RateControlMethod;

        RcParams rcParams;
        SetCalcParams(&rcParams, in);

        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && rcParams.TargetKbps) {
                // last denominator 1000 gives about 0.75 Mbps for 1080p x 30
                Ipp32s minBitRate = (Ipp32s)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                                             * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 1000);
                if (minBitRate > rcParams.TargetKbps) {
                    rcParams.TargetKbps = minBitRate;
                    isCorrected ++;
                }
                Ipp32s AveFrameKB = rcParams.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
                if (rcParams.BufferSizeInKB != 0 && rcParams.BufferSizeInKB < 2 * AveFrameKB) {
                    rcParams.BufferSizeInKB = 2 * AveFrameKB;
                    isCorrected ++;
                }
                if (rcParams.InitialDelayInKB != 0 && rcParams.InitialDelayInKB < AveFrameKB) {
                    rcParams.InitialDelayInKB = AveFrameKB;
                    isCorrected ++;
                }
            }

            if (rcParams.MaxKbps != 0 && rcParams.MaxKbps < rcParams.TargetKbps) {
                rcParams.MaxKbps = rcParams.TargetKbps;
            }
        }
        // check for correct QPs for const QP mode
        else if (out->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
            if (in->mfx.QPI > 51) {
                out->mfx.QPI = 0;
                isInvalid ++;
            } else out->mfx.QPI = in->mfx.QPI;
            if (in->mfx.QPP > 51) {
                out->mfx.QPP = 0;
                isInvalid ++;
            } else out->mfx.QPP = in->mfx.QPP;
            if (in->mfx.QPB > 51) {
                out->mfx.QPB = 0;
                isInvalid ++;
            } else out->mfx.QPB = in->mfx.QPB;
        }
        else {
            out->mfx.Accuracy = in->mfx.Accuracy;
            out->mfx.Convergence = in->mfx.Convergence;
        }

        GetCalcParams(out, &rcParams, out->mfx.RateControlMethod);

        if (opts_in) {
            if ((opts_in->Log2MaxCUSize != 0 && opts_in->Log2MaxCUSize < 4) || opts_in->Log2MaxCUSize > 6) {
                opts_out->Log2MaxCUSize = 0;
                isInvalid ++;
            } else opts_out->Log2MaxCUSize = opts_in->Log2MaxCUSize;

            if ( (opts_in->MaxCUDepth > 5) ||
                 (opts_out->Log2MaxCUSize!=0 && opts_in->MaxCUDepth + 1 > opts_out->Log2MaxCUSize) ) {
                opts_out->MaxCUDepth = 0;
                isInvalid ++;
            } else opts_out->MaxCUDepth = opts_in->MaxCUDepth;

            if (opts_out->Log2MaxCUSize && opts_out->MaxCUDepth) {
                int MinCUSize = 1 << (opts_out->Log2MaxCUSize - opts_out->MaxCUDepth + 1);
                if ((out->mfx.FrameInfo.Width | out->mfx.FrameInfo.Height) & (MinCUSize-1)) {
                    opts_out->MaxCUDepth = 0;
                    isInvalid ++;
                }
            }

            if ( (opts_in->QuadtreeTULog2MaxSize > 5) ||
                 (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTULog2MaxSize > opts_out->Log2MaxCUSize) ) {
                opts_out->QuadtreeTULog2MaxSize = 0;
                isInvalid ++;
            } else opts_out->QuadtreeTULog2MaxSize = opts_in->QuadtreeTULog2MaxSize;

            if ( (opts_in->QuadtreeTULog2MinSize == 1) ||
                 (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTULog2MinSize > opts_out->Log2MaxCUSize) ) {
                opts_out->QuadtreeTULog2MinSize = 0;
                isInvalid ++;
            } else opts_out->QuadtreeTULog2MinSize = opts_in->QuadtreeTULog2MinSize;

            if (opts_out->QuadtreeTULog2MinSize)
                if ((out->mfx.FrameInfo.Width | out->mfx.FrameInfo.Height) & ((1<<opts_out->QuadtreeTULog2MinSize)-1)) {
                    opts_out->QuadtreeTULog2MinSize = 0;
                    isInvalid ++;
                }

            if ( (opts_in->QuadtreeTUMaxDepthIntra > 5) ||
                 (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTUMaxDepthIntra + 1 > opts_out->Log2MaxCUSize) ) {
                opts_out->QuadtreeTUMaxDepthIntra = 0;
                isInvalid ++;
            } else opts_out->QuadtreeTUMaxDepthIntra = opts_in->QuadtreeTUMaxDepthIntra;

            if ( (opts_in->QuadtreeTUMaxDepthInter > 5) ||
                 (opts_out->Log2MaxCUSize!=0 && opts_in->QuadtreeTUMaxDepthInter + 1 > opts_out->Log2MaxCUSize) ) {
                opts_out->QuadtreeTUMaxDepthInter = 0;
                isInvalid ++;
            } else opts_out->QuadtreeTUMaxDepthInter = opts_in->QuadtreeTUMaxDepthInter;

            opts_out->CmIntraThreshold = opts_in->CmIntraThreshold;
            opts_out->TUSplitIntra = opts_in->TUSplitIntra;
            opts_out->CUSplit = opts_in->CUSplit;
            opts_out->IntraAngModes = opts_in->IntraAngModes;
            opts_out->HadamardMe = opts_in->HadamardMe;
            opts_out->TMVP = opts_in->TMVP;
            opts_out->EnableCm = opts_in->EnableCm;
            opts_out->FastPUDecision = opts_in->FastPUDecision;

            CHECK_OPTION(opts_in->AnalyzeChroma, opts_out->AnalyzeChroma, isInvalid);  /* tri-state option */
            CHECK_OPTION(opts_in->SignBitHiding, opts_out->SignBitHiding, isInvalid);  /* tri-state option */
            CHECK_OPTION(opts_in->RDOQuant, opts_out->RDOQuant, isInvalid);            /* tri-state option */
            CHECK_OPTION(opts_in->GPB, opts_out->GPB, isInvalid);            /* tri-state option */
            CHECK_OPTION(opts_in->AMP, opts_out->AMP, isInvalid);            /* tri-state option */
            CHECK_OPTION(opts_in->SAO, opts_out->SAO, isInvalid);            /* tri-state option */
            CHECK_OPTION(opts_in->WPP, opts_out->WPP, isInvalid);       /* tri-state option */
            CHECK_OPTION(opts_in->BPyramid, opts_out->BPyramid, isInvalid);  /* tri-state option */

            if (opts_out->BPyramid == MFX_CODINGOPTION_ON) {
                Ipp32s GopRefDist = out->mfx.GopRefDist;

                while (((GopRefDist & 1) == 0) && GopRefDist > 1)
                    GopRefDist >>= 1;

                if (GopRefDist != 1 || (out->mfx.GopPicSize % out->mfx.GopRefDist)) {
                    opts_out->BPyramid = MFX_CODINGOPTION_OFF; // not supported now
                    isCorrected ++;
                }
            }

            if (opts_out->SignBitHiding == MFX_CODINGOPTION_ON && opts_out->RDOQuant == MFX_CODINGOPTION_ON) { // doesn't work together
                opts_out->SignBitHiding = MFX_CODINGOPTION_OFF;
                isCorrected++;
            }
            if (opts_out->SAO == MFX_CODINGOPTION_ON && in->mfx.NumSlice > 1) { // switch off SAO for now, because of inter-slice deblocking
                opts_out->SAO = MFX_CODINGOPTION_OFF;
                isCorrected++;
            }

            if (opts_in->SplitThresholdStrengthCUIntra > 3) {
                opts_out->SplitThresholdStrengthCUIntra = 0;
                isInvalid ++;
            } else opts_out->SplitThresholdStrengthCUIntra = opts_in->SplitThresholdStrengthCUIntra;

            if (opts_in->SplitThresholdStrengthTUIntra > 3) {
                opts_out->SplitThresholdStrengthTUIntra = 0;
                isInvalid ++;
            } else opts_out->SplitThresholdStrengthTUIntra = opts_in->SplitThresholdStrengthTUIntra;

            if (opts_in->SplitThresholdStrengthCUInter > 3) {
                opts_out->SplitThresholdStrengthCUInter = 0;
                isInvalid ++;
            } else opts_out->SplitThresholdStrengthCUInter = opts_in->SplitThresholdStrengthCUInter;

#define CHECK_NUMCAND(field)                 \
    if (opts_in->field > maxnum) {           \
        opts_out->field = 0;                 \
        isInvalid ++;                        \
    } else opts_out->field = opts_in->field; \
    if (opts_out->field > 0) maxnum = opts_out->field;

            mfxU16 maxnum = 35;

            CHECK_NUMCAND(IntraNumCand1_2)
            CHECK_NUMCAND(IntraNumCand1_3)
            CHECK_NUMCAND(IntraNumCand1_4)
            CHECK_NUMCAND(IntraNumCand1_5)
            CHECK_NUMCAND(IntraNumCand1_6)
            CHECK_NUMCAND(IntraNumCand2_2)
            CHECK_NUMCAND(IntraNumCand2_3)
            CHECK_NUMCAND(IntraNumCand2_4)
            CHECK_NUMCAND(IntraNumCand2_5)
            CHECK_NUMCAND(IntraNumCand2_6)

#undef CHECK_NUMCAND

            // check again Numslice using Log2MaxCUSize
            if ( out->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Width && opts_out->Log2MaxCUSize)
            {
                mfxU16 rnd = (1 << opts_out->Log2MaxCUSize) - 1, shft = opts_out->Log2MaxCUSize;
                if (out->mfx.NumSlice > ((out->mfx.FrameInfo.Height+rnd)>>shft) * ((out->mfx.FrameInfo.Width+rnd)>>shft) )
                {
                    out->mfx.NumSlice = 0;
                    isInvalid ++;
                }
            }

        } // EO mfxExtCodingOptionHEVC

        if (optsDump_in) {
            vm_string_strcpy_s(optsDump_out->ReconFilename, sizeof(optsDump_out->ReconFilename), optsDump_in->ReconFilename);
        }

        // reserved for any case
        ////// Assume 420 checking vertical crop to be even for progressive PicStruct
        ////mfxU16 cropSampleMask, correctCropTop, correctCropBottom;
        ////if ((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE)
        ////    cropSampleMask = 1;
        ////else
        ////    cropSampleMask = 3;
        ////
        ////correctCropTop = (in->mfx.FrameInfo.CropY + cropSampleMask) & ~cropSampleMask;
        ////correctCropBottom = (in->mfx.FrameInfo.CropY + out->mfx.FrameInfo.CropH) & ~cropSampleMask;
        ////
        ////if ((in->mfx.FrameInfo.CropY & cropSampleMask) || (out->mfx.FrameInfo.CropH & cropSampleMask)) {
        ////    if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
        ////        out->mfx.FrameInfo.CropY = correctCropTop;
        ////    if (correctCropBottom >= out->mfx.FrameInfo.CropY)
        ////        out->mfx.FrameInfo.CropH = correctCropBottom - out->mfx.FrameInfo.CropY;
        ////    else // CropY < cropSample
        ////        out->mfx.FrameInfo.CropH = 0;
        ////    isCorrected ++;
        ////}


    }

    if (isInvalid)
        return MFX_ERR_UNSUPPORTED;
    if (isCorrected)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    if (core)
        return (core->GetPlatformType() == MFX_PLATFORM_SOFTWARE)?MFX_ERR_NONE:MFX_WRN_PARTIAL_ACCELERATION;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    // check for valid IOPattern
    mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        (IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->Protected != 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU16 nFrames = IPP_MIN(par->mfx.GopRefDist, H265_MAXREFDIST); // 1 for current and GopRefDist - 1 for reordering

    if( nFrames == 0 ){
        // curr + number of B-frames from target usage
        nFrames = tab_tuGopRefDist[par->mfx.TargetUsage];
    }
    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);
    request->Info = par->mfx.FrameInfo;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY){
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    } else if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (core->GetPlatformType() == MFX_PLATFORM_SOFTWARE)?MFX_ERR_NONE:MFX_WRN_PARTIAL_ACCELERATION;
}

mfxStatus MFXVideoENCODEH265::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));
    stat->NumCachedFrame = m_frameCountBufferedSync; //(mfxU32)m_inFrames.size();
    stat->NumBit = m_totalBits;
    stat->NumFrame = m_encodedFrames;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxBitstream *bitstream = 0;
    //mfxU8* dataPtr;
    mfxU32 initialDataLength = 0;
    mfxFrameSurface1 *surface = inputSurface;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    // inputSurface can be opaque. Get real surface from core
    surface = GetOriginalSurface(inputSurface);

    if (inputSurface != surface) { // input surface is opaque surface
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted = inputSurface->Data.Corrupted;
        surface->Data.DataFlag = inputSurface->Data.DataFlag;
        surface->Info = inputSurface->Info;
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        bitstream = bs;
        //dataPtr = bitstream->Data + bitstream->DataOffset + bitstream->DataLength;
        initialDataLength = bitstream->DataLength;
    }

    if (surface)
    {
        if (surface->Info.FourCC != MFX_FOURCC_NV12)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

        bool locked = false;
        if (m_useAuxInput) { // copy from d3d to internal frame in system memory

            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock
            st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&m_auxInput,
                                           MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                           surface,
                                           MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

            m_core->DecreaseReference(&(surface->Data)); // do it here

            m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &m_auxInput; // replace input pointer
        } else {
            if (surface->Data.Y == 0 && surface->Data.U == 0 &&
                surface->Data.V == 0 && surface->Data.A == 0)
            {
                st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
                if (st == MFX_ERR_NONE)
                    locked = true;
                else
                    return st;
            }
        }

        if (!surface->Data.Y || !surface->Data.UV || surface->Data.Pitch > 0x7fff ||
            surface->Data.Pitch < m_mfxVideoParam.mfx.FrameInfo.Width )
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        mfxRes = m_enc->EncodeFrame(surface, bitstream);
        m_core->DecreaseReference(&(surface->Data));

        if ( m_useAuxInput ) {
            m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        } else {
            if (locked)
                m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
        }
        m_frameCount ++;
        if (mfxRes == MFX_ERR_MORE_DATA)
            m_frameCountBuffered ++;
    } else {
        mfxRes = m_enc->EncodeFrame(NULL, bitstream);
        //        res = Encode(cur_enc, 0, &m_data_out, p_data_out_ext, 0, 0, 0);
        if (mfxRes == MFX_ERR_NONE)
            m_frameCountBuffered --;
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        if (bitstream->DataLength != initialDataLength )
            m_encodedFrames++;
        m_totalBits += (bitstream->DataLength - initialDataLength) * 8;

        if (m_enc->m_pCurrentFrame) {
            bitstream->TimeStamp = m_enc->m_pCurrentFrame->TimeStamp;
            mfxI32 dpb_output_delay = (m_enc->m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B) ? 0 : m_frameCountBuffered + (m_mfxVideoParam.mfx.GopRefDist > 1 ? 1 : 0);
            mfxF64 tcDuration90KHz = (mfxF64)m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD / m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN * 90000; // calculate tick duration
            bitstream->DecodeTimeStamp = mfxI64(bitstream->TimeStamp - tcDuration90KHz * dpb_output_delay); // calculate DTS from PTS

            // Set FrameType
            bs->FrameType = (mfxU16)m_enc->m_pCurrentFrame->m_PicCodType;
            if (m_enc->m_pCurrentFrame->m_bIsIDRPic)
                bs->FrameType |= MFX_FRAMETYPE_IDR;
            if (m_enc->m_pCurrentFrame->m_isShortTermRef || m_enc->m_pCurrentFrame->m_isLongTermRef)
                bs->FrameType |= MFX_FRAMETYPE_REF;
        }
    }

    if (mfxRes == MFX_ERR_MORE_DATA)
        mfxRes = MFX_ERR_NONE;

    return mfxRes;
}

mfxStatus MFXVideoENCODEH265::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par)

    par->mfx = m_mfxVideoParam.mfx;
    par->Protected = 0;
    par->AsyncDepth = m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_mfxVideoParam.IOPattern;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HEVCENC );
    if (opts_hevc)
        *opts_hevc = m_mfxHEVCOpts;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::GetFrameParam(mfxFrameParam * /*par*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVideoENCODEH265::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)
{
    pEntryPoint->pRoutine = TaskRoutine;
    pEntryPoint->pCompleteProc = TaskCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->requiredNumThreads = m_mfxVideoParam.mfx.NumThread;
    pEntryPoint->pRoutineName = "EncodeHEVC";

    mfxStatus status = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);

    mfxFrameSurface1 *realSurface = GetOriginalSurface(surface);

    if (surface && !realSurface)
        return MFX_ERR_UNDEFINED_BEHAVIOR;


    if (MFX_ERR_NONE == status || MFX_ERR_MORE_DATA_RUN_TASK == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status || MFX_ERR_MORE_BITSTREAM == status)
    {
        // lock surface. If input surface is opaque core will lock both opaque and associated realSurface
        if (realSurface) {
            m_core->IncreaseReference(&(realSurface->Data));
        }
        H265EncodeTaskInputParams *m_pTaskInputParams = (H265EncodeTaskInputParams*)H265_Malloc(sizeof(H265EncodeTaskInputParams));
        // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later. Output bitstream isn't required for this task
        m_pTaskInputParams->bs = (status == MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
        m_pTaskInputParams->ctrl = ctrl;
        m_pTaskInputParams->surface = surface;
        pEntryPoint->pParam = m_pTaskInputParams;
    }

    return status;

} // mfxStatus MFXVideoENCODEVP8::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)


mfxStatus MFXVideoENCODEH265::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    H265ENC_UNREFERENCED_PARAMETER(threadNumber);
    H265ENC_UNREFERENCED_PARAMETER(callNumber);

    if (pState == NULL || pParam == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    MFXVideoENCODEH265 *th = static_cast<MFXVideoENCODEH265 *>(pState);

    H265EncodeTaskInputParams *pTaskParams = (H265EncodeTaskInputParams*)pParam;

    if (th->m_taskParams.parallel_encoding_finished)
    {
        return MFX_TASK_DONE;
    }

    mfxI32 single_thread_selected = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.single_thread_selected), 1, 0);
    if (!single_thread_selected)
    {
        // This task performs all single threaded regions
        mfxStatus status = th->EncodeFrame(pTaskParams->ctrl, NULL, pTaskParams->surface, pTaskParams->bs);
        th->m_taskParams.parallel_encoding_finished = true;
        H265_Free(pParam);

        return status;
    }
    else
    {
        vm_mutex_lock(&th->m_taskParams.parallel_region_end_lock);
        if (th->m_taskParams.parallel_execution_in_progress)
        {
            // Here we get only when thread that performs single thread work sent event
            // In this case m_taskParams.thread_number is set to 0, this is the number of single thread task
            // All other tasks receive their number > 0
            mfxI32 thread_number = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.thread_number));

            mfxStatus status = MFX_ERR_NONE;

            // Some thread finished its work and was called again, but there is no work left for it since
            // if it finished, the only work remaining is being done by other currently working threads. No
            // work is possible so thread goes away waiting for maybe another parallel region
            if (thread_number > th->m_taskParams.num_threads)
            {
                vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.thread_number));
                vm_mutex_unlock(&th->m_taskParams.parallel_region_end_lock);
                return MFX_TASK_BUSY;
            }

            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));
            vm_mutex_unlock(&th->m_taskParams.parallel_region_end_lock);

            mfxI32 parallel_region_selector = th->m_taskParams.parallel_region_selector;

            if (parallel_region_selector == PARALLEL_REGION_MAIN) {
                if (th->m_enc->m_videoParam.threading_by_rows)
                    th->m_enc->EncodeThreadByRow(thread_number);
                else
                    th->m_enc->EncodeThread(thread_number);
            } else if (parallel_region_selector == PARALLEL_REGION_DEBLOCKING)
                th->m_enc->DeblockThread(thread_number);

            vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));

            // We're done, let single thread to continue
            vm_event_signal(&th->m_taskParams.parallel_region_done);

            if (MFX_ERR_NONE == status)
            {
                return MFX_TASK_WORKING;
            }
            else
            {
                return status;
            }
        }
        else
        {
            vm_mutex_unlock(&th->m_taskParams.parallel_region_end_lock);
            return MFX_TASK_BUSY;
        }
    }

} // mfxStatus MFXVideoENCODEVP8::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)


mfxStatus MFXVideoENCODEH265::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    H265ENC_UNREFERENCED_PARAMETER(pParam);
    H265ENC_UNREFERENCED_PARAMETER(taskRes);
    if (pState == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    MFXVideoENCODEH265 *th = static_cast<MFXVideoENCODEH265 *>(pState);

    th->m_taskParams.single_thread_selected = 0;
    th->m_taskParams.thread_number = 0;
    th->m_taskParams.parallel_encoding_finished = false;
    th->m_taskParams.parallel_execution_in_progress = false;

    return MFX_TASK_DONE;

} // mfxStatus MFXVideoENCODEVP8::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)

void MFXVideoENCODEH265::ParallelRegionStart(Ipp32s num_threads, Ipp32s region_selector)
{
    m_taskParams.parallel_region_selector = region_selector;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_executing_threads = 0;
    m_taskParams.num_threads = num_threads;
    m_taskParams.parallel_execution_in_progress = true;

    // Signal scheduler that all busy threads should be unleashed
    m_core->INeedMoreThreadsInside(this);

} // void MFXVideoENCODEVP8::ParallelRegionStart(threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size)


void MFXVideoENCODEH265::ParallelRegionEnd()
{
    vm_mutex_lock(&m_taskParams.parallel_region_end_lock);
    // Disable parallel task execution
    m_taskParams.parallel_execution_in_progress = false;
    vm_mutex_unlock(&m_taskParams.parallel_region_end_lock);

    // Wait for other threads to finish encoding their last macroblock row
    while(m_taskParams.parallel_executing_threads > 0)
    {
        vm_event_wait(&m_taskParams.parallel_region_done);
    }

} // void MFXVideoENCODEVP8::ParallelRegionEnd()

mfxFrameSurface1* MFXVideoENCODEH265::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaque)
        return m_core->GetNativeSurface(surface);
    return surface;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
