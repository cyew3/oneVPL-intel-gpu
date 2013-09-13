//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfxdefs.h"
#include "mfx_h265_encode.h"
#include "mfx_common_int.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "vm_event.h"
#include "mfx_ext_buffers.h"
#include <new>

#include "mfx_h265_defs.h"
#include "umc_structures.h"
#include "mfx_enc_common.h"


//////////////////////////////////////////////////////////////////////////

#define H265ENC_UNREFERENCED_PARAMETER(X) X=X

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)

#define CHECK_OPTION(input, output, corcnt) \
  if ( input != MFX_CODINGOPTION_OFF &&     \
      input != MFX_CODINGOPTION_ON  &&      \
      input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;      \
    (corcnt) ++;                            \
  } else output = input;

#define CHECK_EXTBUF_SIZE(ebuf, errcounter) if ((ebuf).Header.BufferSz != sizeof(ebuf)) {(errcounter) = (errcounter) + 1;}

mfxExtCodingOptionHEVC hevc_tu_tab[8] = {               // CUS CUD 2TUS 2TUD  AnalyzeChroma         SignBitHiding          RDOQuant              thrCU,TU 5numCand1  5numCand2  WPP
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu default (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 6,  4, 5,2, 5,5,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_ON,  0, 0,    8,8,4,4,4, 4,4,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 1
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 2  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 3  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 4
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 5  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  3, 4,2, 3,3,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_ON,   MFX_CODINGOPTION_OFF, 1, 1,    6,6,3,3,3, 3,3,2,2,2, MFX_CODINGOPTION_UNKNOWN }, // tu 6  (==4)
    {{MFX_EXTBUFF_HEVCENC, sizeof(mfxExtCodingOptionHEVC)}, 5,  2, 4,2, 2,2,  MFX_CODINGOPTION_ON,  MFX_CODINGOPTION_OFF,  MFX_CODINGOPTION_OFF, 2, 2,    4,4,2,2,2, 2,2,1,1,1, MFX_CODINGOPTION_UNKNOWN }  // tu 7
};

#define H265_MAXREFDIST 1


//////////////////////////////////////////////////////////////////////////

void mfxVideoH265InternalParam::SetCalcParams( mfxVideoParam *parMFX) {

    mfxU16 mult = IPP_MAX( parMFX->mfx.BRCParamMultiplier, 1);

    calcParam.TargetKbps = parMFX->mfx.TargetKbps * mult;
    calcParam.MaxKbps = parMFX->mfx.MaxKbps * mult;
    calcParam.BufferSizeInKB = parMFX->mfx.BufferSizeInKB * mult;
    calcParam.InitialDelayInKB = parMFX->mfx.InitialDelayInKB * mult;
}
void mfxVideoH265InternalParam::GetCalcParams( mfxVideoParam *parMFX) {

    mfxU32 maxVal = IPP_MAX( IPP_MAX( calcParam.TargetKbps, calcParam.MaxKbps), IPP_MAX( calcParam.BufferSizeInKB, calcParam.InitialDelayInKB));
    mfxU16 mult = (mfxU16)((maxVal + 0xffff) / 0x10000);

    if (mult) {
        parMFX->mfx.BRCParamMultiplier = mult;
        parMFX->mfx.TargetKbps = (mfxU16)(calcParam.TargetKbps / mult);
        parMFX->mfx.MaxKbps = (mfxU16)(calcParam.MaxKbps / mult);
        parMFX->mfx.BufferSizeInKB = (mfxU16)(calcParam.BufferSizeInKB / mult);
        parMFX->mfx.InitialDelayInKB = (mfxU16)(calcParam.InitialDelayInKB / mult);
    }
}

mfxVideoH265InternalParam::mfxVideoH265InternalParam()
{
    memset(this, 0, sizeof(*this));
}

mfxVideoH265InternalParam::mfxVideoH265InternalParam(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
}

mfxVideoH265InternalParam& mfxVideoH265InternalParam::operator=(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
    return *this;
}

inline Ipp32u h265enc_ConvertBitrate(mfxU16 TargetKbps)
{
    return (TargetKbps * 1000);
}

// check for known ExtBuffers, returns error code. or -1 if found unknown
// zero mfxExtBuffer* are OK
mfxStatus CheckExtBuffers_H265enc(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{

    mfxU32 ID_list[] = { MFX_EXTBUFF_HEVCENC, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION };

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

    if( bs->MaxLength - bs->DataOffset - bs->DataLength < m_mfxVideoParam.mfx.BufferSizeInKB)
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

    mfxVideoH265InternalParam inInt = *par_in;
    mfxVideoH265InternalParam *par = &inInt;

    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtBuffer *ptr_checked_ext[2] = {0};
    mfxU16 ext_counter = 0;
    memset(&m_mfxVideoParam,0,sizeof(mfxVideoParam));
    memset(&m_mfxHEVCOpts,0,sizeof(m_mfxHEVCOpts));

    if (opts_hevc) {
        m_mfxHEVCOpts.Header.BufferId = MFX_EXTBUFF_HEVCENC;
        m_mfxHEVCOpts.Header.BufferSz = sizeof(m_mfxHEVCOpts);
        ptr_checked_ext[ext_counter++] = &m_mfxHEVCOpts.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    m_mfxVideoParam.ExtParam = ptr_checked_ext;
    m_mfxVideoParam.NumExtParam = ext_counter;

    stsQuery = Query(par_in, &m_mfxVideoParam); // [has to] copy all provided params

    // return status for Init differs in these cases
    if (stsQuery == MFX_ERR_UNSUPPORTED &&
         ( (par_in->mfx.NumSlice > 0 && m_mfxVideoParam.mfx.NumSlice == 0) ||
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
            memset(&m_response_alien, 0, sizeof(m_response_alien));
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

            sts = m_core->AllocFrames(&request,
                                     &m_response_alien,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);

            if (MFX_ERR_NONE != sts &&
                MFX_ERR_UNSUPPORTED != sts) // unsupported means that current Core couldn;t allocate the surfaces
                return sts;

            if (m_response_alien.NumFrameActual < request.NumFrameMin)
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



    mfxExtCodingOptionHEVC* opts_tu = &hevc_tu_tab[m_mfxVideoParam.mfx.TargetUsage];

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
        m_mfxHEVCOpts = hevc_tu_tab[m_mfxVideoParam.mfx.TargetUsage];
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

    }

    // sign bit hiding doesn't work now properly
    m_mfxHEVCOpts.SignBitHiding = MFX_CODINGOPTION_OFF;

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

    // CodecId - only HEVC;  CodecProfile CodecLevel - ignore
    // NumThread - set inside, >1 only if WPP
    // TargetUsage - nothing to do

    // can depend on target usage
    if (!m_mfxVideoParam.mfx.GopPicSize) m_mfxVideoParam.mfx.GopPicSize = (mfxU16)
       (( m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN + m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD - 1 ) / m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD);
    if (!m_mfxVideoParam.mfx.GopRefDist) m_mfxVideoParam.mfx.GopRefDist = 1;
    if (!m_mfxVideoParam.mfx.IdrInterval) m_mfxVideoParam.mfx.IdrInterval = m_mfxVideoParam.mfx.GopPicSize * 8;
    // GopOptFlag ignore

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
    if (!m_mfxVideoParam.mfx.NumRefFrame) m_mfxVideoParam.mfx.NumRefFrame = 1; // for now


    // what is still needed?
    //m_mfxVideoParam.mfx = par->mfx;
    m_mfxVideoParam.SetCalcParams(&m_mfxVideoParam);
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

    m_isInitialized = true;

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
        m_core->FreeFrames(&m_response_alien);
        m_response_alien.NumFrameActual = 0;
        m_useVideoOpaq = false;
    }


    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Reset(mfxVideoParam *par_in)
{
    mfxStatus sts;

    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    sts = CheckExtBuffers_H265enc( par_in->ExtParam, par_in->NumExtParam );
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    mfxVideoH265InternalParam inInt = *par_in;
    //mfxVideoH265InternalParam *par = &inInt;

    mfxVideoParam checked_videoParam = *par_in;
    mfxExtCodingOptionHEVC checked_codingOptionHEVC = *opts_hevc;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;

    mfxExtBuffer *ptr_checked_ext[2] = {0};
    mfxU16 ext_counter = 0;


    if (opts_hevc) {
        checked_codingOptionHEVC = *opts_hevc;
        ptr_checked_ext[ext_counter++] = &checked_codingOptionHEVC.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    m_mfxVideoParam.ExtParam = ptr_checked_ext;
    m_mfxVideoParam.NumExtParam = ext_counter;

    sts/*Query*/ = Query(par_in, &checked_videoParam); // [has to] copy all provided params

    MFX_CHECK_STS(sts);

    // Not finished implementation

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_frameCountBufferedSync = 0;
    m_frameCountBuffered = 0;

//    sts = m_enc->Reset();
//    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Query(mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    mfxStatus st;
    MFX_CHECK_NULL_PTR1(par_out)
    CHECK_VERSION(par_in->Version);
    CHECK_VERSION(par_out->Version);

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
            optsHEVC->SplitThresholdStrengthCU = 1;
            optsHEVC->SplitThresholdStrengthTU = 1;
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
        }

        // ignore all reserved
    } else { //Check options for correctness
        mfxVideoH265InternalParam inInt = *par_in;
        mfxVideoH265InternalParam outInt = *par_out;
        mfxVideoH265InternalParam *in = &inInt;
        mfxVideoH265InternalParam *out = &outInt;

        if ( in->mfx.CodecId != MFX_CODEC_HEVC)
            return MFX_ERR_UNSUPPORTED;

        st = CheckExtBuffers_H265enc( in->ExtParam, in->NumExtParam );
        MFX_CHECK_STS(st);
        st = CheckExtBuffers_H265enc( out->ExtParam, out->NumExtParam );
        MFX_CHECK_STS(st);

        const mfxExtCodingOptionHEVC* opts_in  = (mfxExtCodingOptionHEVC*)GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_HEVCENC );
        mfxExtCodingOptionHEVC*       opts_out = (mfxExtCodingOptionHEVC*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_HEVCENC );

        if (opts_in           ) CHECK_EXTBUF_SIZE( *opts_in,            isInvalid)
        if (opts_out          ) CHECK_EXTBUF_SIZE( *opts_out,           isInvalid)
        if (isInvalid)
            return MFX_ERR_UNSUPPORTED;

        if ((opts_in==0) != (opts_out==0))
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
            isCorrected ++;
        }
        out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;

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
        out->mfx.EncodedOrder = 0; //in->mfx.EncodedOrder;

        // RC

        if (in->mfx.RateControlMethod != 0 &&
            in->mfx.RateControlMethod != MFX_RATECONTROL_CBR && in->mfx.RateControlMethod != MFX_RATECONTROL_VBR && in->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
            out->mfx.RateControlMethod = 0;
            isInvalid ++;
        } else out->mfx.RateControlMethod = in->mfx.RateControlMethod;

        out->calcParam.BufferSizeInKB = in->calcParam.BufferSizeInKB;
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            out->calcParam.TargetKbps = in->calcParam.TargetKbps;
            out->calcParam.InitialDelayInKB = in->calcParam.InitialDelayInKB;
            if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && out->calcParam.TargetKbps) {
                // last denominator 1000 gives about 0.75 Mbps for 1080p x 30
                mfxU32 minBitRate = (mfxU32)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                                             * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 1000);
                if (minBitRate > out->calcParam.TargetKbps) {
                    out->calcParam.TargetKbps = (mfxU32)minBitRate;
                    isCorrected ++;
                }
                mfxU32 AveFrameKB = out->calcParam.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
                if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB < 2 * AveFrameKB) {
                    out->calcParam.BufferSizeInKB = (mfxU32)(2 * AveFrameKB);
                    isCorrected ++;
                }
                if (out->calcParam.InitialDelayInKB != 0 && out->calcParam.InitialDelayInKB < AveFrameKB) {
                    out->calcParam.InitialDelayInKB = (mfxU32)AveFrameKB;
                    isCorrected ++;
                }
            }

            if (in->calcParam.MaxKbps != 0 && in->calcParam.MaxKbps < out->calcParam.TargetKbps) {
                out->calcParam.MaxKbps = out->calcParam.TargetKbps;
            } else
                out->calcParam.MaxKbps = in->calcParam.MaxKbps;
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
            out->calcParam.TargetKbps = in->calcParam.TargetKbps;
            out->mfx.Accuracy = in->mfx.Accuracy;
            out->mfx.Convergence = in->mfx.Convergence;
        }

        *par_out = *out;
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            out->GetCalcParams(par_out);



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

            CHECK_OPTION(opts_in->AnalyzeChroma, opts_out->AnalyzeChroma, isInvalid);  /* tri-state option */
            CHECK_OPTION(opts_in->SignBitHiding, opts_out->SignBitHiding, isInvalid);  /* tri-state option */
            CHECK_OPTION(opts_in->RDOQuant, opts_out->RDOQuant, isInvalid);            /* tri-state option */

            if (opts_out->SignBitHiding == MFX_CODINGOPTION_ON && opts_out->RDOQuant == MFX_CODINGOPTION_ON) { // doesn't work together
                opts_out->SignBitHiding = MFX_CODINGOPTION_OFF;
                isCorrected++;
            }

            if (opts_in->SplitThresholdStrengthCU > 3) {
                opts_out->SplitThresholdStrengthCU = 0;
                isInvalid ++;
            } else opts_out->SplitThresholdStrengthCU = opts_in->SplitThresholdStrengthCU;

            if (opts_in->SplitThresholdStrengthTU > 3) {
                opts_out->SplitThresholdStrengthTU = 0;
                isInvalid ++;
            } else opts_out->SplitThresholdStrengthTU = opts_in->SplitThresholdStrengthTU;

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

            CHECK_OPTION(opts_in->WPP, opts_out->WPP, isInvalid);  /* tri-state option */

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

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
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

    mfxU16 nFrames = IPP_MAX(par->mfx.GopRefDist, H265_MAXREFDIST); // 1 for current and GopRefDist - 1 for reordering

    if( nFrames == 0 ){
        nFrames = 1; // 1 for current
        // to add if B-frames from target usage
    }
    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);
    request->Info = par->mfx.FrameInfo;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY){
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }else if(par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
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

//            m_core->DecreaseReference(&(surface->Data)); // not here to keep related mfxEncodeCtrl

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
    } else {
        mfxRes = m_enc->EncodeFrame(NULL, bitstream);
        //        res = Encode(cur_enc, 0, &m_data_out, p_data_out_ext, 0, 0, 0);
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        if (bitstream->DataLength != initialDataLength )
            m_encodedFrames++;
        m_totalBits += (bitstream->DataLength - initialDataLength) * 8;
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

            if (parallel_region_selector == PARALLEL_REGION_MAIN)
                th->m_enc->EncodeThread(thread_number);
            else if (parallel_region_selector == PARALLEL_REGION_DEBLOCKING)
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
