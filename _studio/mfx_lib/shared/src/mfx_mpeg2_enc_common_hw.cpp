//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_ENABLE_MPEG2_VIDEO_ENC)|| defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "assert.h"
//#include "mfx_mpeg2_enc_ddi_hw.h"
#include "mfx_enc_common.h"
#include "mfx_mpeg2_encode_interface.h"

#include <memory>

//#define MFX_VA
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"

#ifndef __SW_ENC
using namespace MfxHwMpeg2Encode;

//static const GUID DXVA2_Intel_Auxiliary_Device = 
//{ 0xa74ccae2, 0xf466, 0x45ae, { 0x86, 0xf5, 0xab, 0x8b, 0xe8, 0xaf, 0x84, 0x83 } };

//static const GUID DXVA2_Intel_Encode_MPEG2 = 
//{ 0xc346e8a3, 0xcbed, 0x4d27, { 0x87, 0xcc, 0xa7, 0xe, 0xb4, 0xdc, 0x8c, 0x27 } };

// {55BF20CF-6FF1-4fb4-B08B-33E97E15BA29}
//static const GUID DXVA2_IntelEncode = 
//{ 0x55bf20cf, 0x6ff1, 0x4fb4, { 0xb0, 0x8b, 0x33, 0xe9, 0x7e, 0x15, 0xba, 0x29 } };

// {C346E8A3-CBED-4d27-87CC-A70EB4DC8C27}
//static const GUID INTEL_ENCODE_GUID_MPEG2_VME = 
//{ 0xc346e8a3, 0xcbed, 0x4d27, { 0x87, 0xcc, 0xa7, 0xe, 0xb4, 0xdc, 0x8c, 0x27 } };

// !!! This device GUID is not reported by the GetVideoProcessorDeviceGuids() method.
// {44dee63b-77e6-4dd2-b2e1-443b130f2e2f}

//static const GUID DXVA2_Registration_Device = 
//{ 0x44dee63b, 0x77e6, 0x4dd2, { 0xb2, 0xe1, 0x44, 0x3b, 0x13, 0x0f, 0x2e, 0x2f } };

// from "Intel DXVA Encoding DDI for Vista rev 0.77"
/*enum
{
    ENCODE_ENC_ID                           = 0x100,
    ENCODE_PAK_ID                           = 0x101,
    ENCODE_ENC_PAK_ID                       = 0x102,
    ENCODE_VPP_ID                           = 0x103, // reserved for now

    ENCODE_FORMAT_COUNT_ID                  = 0x104,
    ENCODE_FORMATS_ID                       = 0x105,
    ENCODE_ENC_CTRL_CAPS_ID                 = 0x106,
    ENCODE_ENC_CTRL_GET_ID                  = 0x107,
    ENCODE_ENC_CTRL_SET_ID                  = 0x108,
    ENCODE_INSERT_DATA_ID                   = 0x120,
    ENCODE_QUERY_STATUS_ID                  = 0x121
};*/

namespace MfxHwMpeg2Encode
{
    // aya: declaration 
    bool ConvertFrameRateMPEG2(mfxU32 FrameRateExtD, mfxU32 FrameRateExtN, mfxI32 &frame_rate_code, mfxI32 &frame_rate_extension_n, mfxI32 &frame_rate_extension_d);
}

// From "Intel DXVA2 Auxiliary Functionality Device rev 0.6"
/*typedef enum
{
    AUXDEV_GET_ACCEL_GUID_COUNT             = 1,
    AUXDEV_GET_ACCEL_GUIDS                  = 2,
    AUXDEV_GET_ACCEL_RT_FORMAT_COUNT        = 3,
    AUXDEV_GET_ACCEL_RT_FORMATS             = 4,
    AUXDEV_GET_ACCEL_FORMAT_COUNT           = 5,
    AUXDEV_GET_ACCEL_FORMATS                = 6,
    AUXDEV_QUERY_ACCEL_CAPS                 = 7,
    AUXDEV_CREATE_ACCEL_SERVICE             = 8,
    AUXDEV_DESTROY_ACCEL_SERVICE            = 9
} AUXDEV_FUNCTION_ID;*/

//#define D3DFMT_NV12 (D3DFORMAT)(MAKEFOURCC('N', 'V', '1', '2'))
//#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MAKEFOURCC('N', 'V', '1', '2'))
//#define D3DDDIFMT_YU12 (D3DDDIFORMAT)(MAKEFOURCC('Y', 'U', '1', '2'))

typedef struct tagENCODE_QUERY_STATUS_DATA_tmp
{
    UINT    uBytesWritten;
} ENCODE_QUERY_STATUS_DATA_tmp;

// from "Intel DXVA Encoding DDI for Vista rev 0.77"
/*typedef struct tagENCODE_QUERY_STATUS_PARAMS
{
    UINT    uFrameNum;
    void*   pStatusData;
} ENCODE_QUERY_STATUS_PARAMS;*/


using namespace MfxHwMpeg2Encode;

mfxStatus MfxHwMpeg2Encode::QueryHwCaps(VideoCORE* pCore, ENCODE_CAPS & hwCaps)
{
    // FIXME: remove this when driver starts returning actual encode caps
    /*hwCaps.SliceIPBOnly     = 1;
    hwCaps.EncodeFunc       = 1;
    hwCaps.MaxNum_Reference = 1;
    hwCaps.MaxPicWidth      = 4096;
    hwCaps.MaxPicHeight     = 4096;*/

    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(pCore); 
    if (!pEncodeCaps)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    else
    {
        if (pEncodeCaps->GetHWCaps<ENCODE_CAPS>(DXVA2_Intel_Encode_MPEG2, &hwCaps) == MFX_ERR_NONE)
            return MFX_ERR_NONE;
    }

    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset( CreatePlatformMpeg2Encoder(pCore) );
    if(ddi.get() == NULL)
        return MFX_ERR_NULL_PTR;
    mfxStatus sts = ddi.get()->QueryEncodeCaps(hwCaps);
    MFX_CHECK_STS(sts);

    return pEncodeCaps->SetHWCaps<ENCODE_CAPS>(DXVA2_Intel_Encode_MPEG2, &hwCaps);
}

#ifdef PAVP_SUPPORT
AesCounter::AesCounter()
{
   memset(this,0,sizeof(this));
}

void AesCounter::Init(const mfxExtPAVPOption & opt)
{
    m_opt = opt;
    Count = opt.CipherCounter.Count;
    IV    = opt.CipherCounter.IV;
}

static mfxU64 SwapEndian(mfxU64 val)
{
    return
        ((val << 56) & 0xff00000000000000) | ((val << 40) & 0x00ff000000000000) |
        ((val << 24) & 0x0000ff0000000000) | ((val <<  8) & 0x000000ff00000000) |
        ((val >>  8) & 0x00000000ff000000) | ((val >> 24) & 0x0000000000ff0000) |
        ((val >> 40) & 0x000000000000ff00) | ((val >> 56) & 0x00000000000000ff);
}


bool AesCounter::Increment()
{
    if (m_opt.EncryptionType == MFX_PAVP_AES128_CTR)
    {
        mfxU64 tmp = SwapEndian(Count) + m_opt.CounterIncrement;

        if (m_opt.CounterType == MFX_PAVP_CTR_TYPE_A)
            tmp &= 0xffffffff;

        m_wrapped = (tmp < m_opt.CounterIncrement);
        Count = SwapEndian(tmp);
    }

    return m_wrapped;
}
#endif // PAVP_SUPPORT

//-------------------------------------------------------------
//  Execute Buffers
//-------------------------------------------------------------

#ifdef MFX_UNDOCUMENTED_QUANT_MATRIX
#if defined(MFX_VA_WIN)
static
void SetQMParameters(const mfxExtCodingOptionQuantMatrix*  pMatrix, 
                     ENCODE_SET_PICTURE_QUANT &quantMatrix)
{
    memset(&quantMatrix,0, sizeof(quantMatrix));

    quantMatrix.QmatrixMPEG2.bNewQmatrix[0] = pMatrix->bIntraQM && pMatrix->IntraQM[0];
    quantMatrix.QmatrixMPEG2.bNewQmatrix[1] = pMatrix->bInterQM && pMatrix->InterQM[0];
    quantMatrix.QmatrixMPEG2.bNewQmatrix[2] = pMatrix->bChromaIntraQM && pMatrix->ChromaIntraQM[0];
    quantMatrix.QmatrixMPEG2.bNewQmatrix[3] = pMatrix->bChromaInterQM && pMatrix->ChromaInterQM[0];

    const mfxU8* pQmatrix[4]    = {pMatrix->IntraQM,  pMatrix->InterQM,   pMatrix->ChromaIntraQM, pMatrix->ChromaInterQM};

    for (int blk_type = 0; blk_type < 4; blk_type ++)
    {
        if (quantMatrix.QmatrixMPEG2.bNewQmatrix[blk_type])
        {
            for (int i = 0; i < 64; i ++)
            {
                quantMatrix.QmatrixMPEG2.Qmatrix[blk_type][i] = pQmatrix[blk_type][i] ? pQmatrix[blk_type][i] : quantMatrix.QmatrixMPEG2.Qmatrix[blk_type][i-1];        
            }
        }      
    }

}

#else

static
void SetQMParameters(const mfxExtCodingOptionQuantMatrix*  pMatrix, 
                    VAIQMatrixBufferMPEG2 &quantMatrix)
{
    memset(&quantMatrix,0, sizeof(quantMatrix));
    
    bool bNewQmatrix[4] = 
    {
        pMatrix->bIntraQM && pMatrix->IntraQM[0],
        pMatrix->bInterQM && pMatrix->InterQM[0],
        pMatrix->bChromaIntraQM && pMatrix->ChromaIntraQM[0],
        pMatrix->bChromaInterQM && pMatrix->ChromaInterQM[0]
    };

    quantMatrix.load_intra_quantiser_matrix = bNewQmatrix[0];
    quantMatrix.load_non_intra_quantiser_matrix = bNewQmatrix[1];
    quantMatrix.load_chroma_intra_quantiser_matrix = bNewQmatrix[2];
    quantMatrix.load_chroma_non_intra_quantiser_matrix = bNewQmatrix[3];

    const mfxU8* pQmatrix[4] = {pMatrix->IntraQM,  pMatrix->InterQM,   pMatrix->ChromaIntraQM, pMatrix->ChromaInterQM};
    mfxU8 * pDestMatrix[4] = 
    {
        quantMatrix.intra_quantiser_matrix, 
        quantMatrix.non_intra_quantiser_matrix,
        quantMatrix.chroma_intra_quantiser_matrix,
        quantMatrix.chroma_non_intra_quantiser_matrix
    };

    for (int blk_type = 0; blk_type < 4; blk_type++)
    {
        if (bNewQmatrix[blk_type])
        {
            for (int i = 0; i < 64; i++)
            {
                // note: i-1 would not overflow due to bNewMatrix definition
                pDestMatrix[blk_type][i] = pQmatrix[blk_type][i] ? pQmatrix[blk_type][i] : pDestMatrix[blk_type][i-1]; 
            }
        }      
    }

}

#endif
#endif // MFX_UNDOCUMENTED_QUANT_MATRIX

mfxStatus ExecuteBuffers::Init(const mfxVideoParamEx_MPEG2* par, mfxU32 funcId, bool bAllowBRC)
{
    DWORD nSlices = par->mfxVideoParams.mfx.NumSlice;
    DWORD nMBs    = (par->mfxVideoParams.mfx.FrameInfo.Width>>4)*(par->mfxVideoParams.mfx.FrameInfo.Height>>4);

    memset(&m_caps, 0, sizeof(m_caps));
    memset(&m_sps,  0, sizeof(m_sps));
    memset(&m_pps,  0, sizeof(m_pps));

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    m_bTriggerGpuHang = false;
#endif
    m_bOutOfRangeMV = false;
    m_bErrMBType    = false;
    m_bUseRawFrames = par->bRawFrames;
    m_fFrameRate = (Ipp64f) par->mfxVideoParams.mfx.FrameInfo.FrameRateExtN/(Ipp64f)par->mfxVideoParams.mfx.FrameInfo.FrameRateExtD;

    m_idxMb = (DWORD(-1));
    m_idxBs = (DWORD(-1));

    if (m_pSlice == 0)
    { 
        m_nSlices = nSlices;
        m_pSlice  = new ENCODE_SET_SLICE_HEADER_MPEG2 [m_nSlices];
        memset (m_pSlice,0,sizeof(ENCODE_SET_SLICE_HEADER_MPEG2)*m_nSlices);
    }
    else
    {
        if (m_nSlices < nSlices)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (m_nMBs == 0)
    { 
        m_nMBs  = nMBs;
        m_pMBs  = new ENCODE_ENC_MB_DATA_MPEG2 [m_nMBs];
        memset (m_pMBs,0,sizeof(ENCODE_ENC_MB_DATA_MPEG2)*m_nMBs);
    }
    else
    {
        if (m_nMBs < nMBs)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
   // m_sps parameters

    m_bAddSPS = 1;

    m_bAddDisplayExt = par->bAddDisplayExt;
    if (m_bAddDisplayExt)
    {
        m_VideoSignalInfo = par->videoSignalInfo;
    }

    if (funcId == ENCODE_ENC_ID)
    {
        m_sps.FrameWidth   = par->mfxVideoParams.mfx.FrameInfo.Width;
        m_sps.FrameHeight  = par->mfxVideoParams.mfx.FrameInfo.Height;
    }
    else if (funcId == ENCODE_ENC_PAK_ID)
    {
        m_sps.FrameWidth = (par->mfxVideoParams.mfx.FrameInfo.CropW!=0) ?
            par->mfxVideoParams.mfx.FrameInfo.CropW:
            par->mfxVideoParams.mfx.FrameInfo.Width;

        m_sps.FrameHeight = (par->mfxVideoParams.mfx.FrameInfo.CropH!=0) ?
            par->mfxVideoParams.mfx.FrameInfo.CropH:
            par->mfxVideoParams.mfx.FrameInfo.Height;
    }
    else
        return MFX_ERR_UNSUPPORTED;

    m_sps.Profile      = (UCHAR)par->mfxVideoParams.mfx.CodecProfile;
    m_sps.Level        = (UCHAR)par->mfxVideoParams.mfx.CodecLevel;
    m_sps.ChromaFormat = (UCHAR)par->mfxVideoParams.mfx.FrameInfo.ChromaFormat;
    m_sps.TargetUsage  = (UCHAR)par->mfxVideoParams.mfx.TargetUsage;
    m_sps.progressive_sequence = par->mfxVideoParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE? 1:0;
    m_GOPPictureSize = par->mfxVideoParams.mfx.GopPicSize;
    m_GOPRefDist     = (UCHAR)par->mfxVideoParams.mfx.GopRefDist;
    m_GOPOptFlag     = (UCHAR)par->mfxVideoParams.mfx.GopOptFlag;

    {
        mfxU32 aw = (par->mfxVideoParams.mfx.FrameInfo.AspectRatioW != 0) ? par->mfxVideoParams.mfx.FrameInfo.AspectRatioW * m_sps.FrameWidth :m_sps.FrameWidth;
        mfxU32 ah = (par->mfxVideoParams.mfx.FrameInfo.AspectRatioH != 0) ? par->mfxVideoParams.mfx.FrameInfo.AspectRatioH * m_sps.FrameHeight:m_sps.FrameHeight;
        mfxU16 ar_code = 1;
        mfxI32 fr_code = 0, fr_codeN = 0, fr_codeD = 0;

        if(aw*3 == ah*4)
            ar_code = 2;
        else if(aw*9 == ah*16)
            ar_code = 3;
        else if(aw*100 == ah*221)
            ar_code = 4;
        else
            ar_code = 1;

        m_sps.AspectRatio = ar_code;

        if (!ConvertFrameRateMPEG2( par->mfxVideoParams.mfx.FrameInfo.FrameRateExtD, 
                                    par->mfxVideoParams.mfx.FrameInfo.FrameRateExtN, 
                                    fr_code, fr_codeN, fr_codeD))
        {
          return MFX_ERR_UNSUPPORTED;
        }

        m_sps.FrameRateCode = (USHORT) fr_code;
        m_sps.FrameRateExtD = (USHORT) fr_codeD;
        m_sps.FrameRateExtN = (USHORT) fr_codeN;

        mfxU32 multiplier = IPP_MAX(par->mfxVideoParams.mfx.BRCParamMultiplier, 1); 

        m_sps.bit_rate = (par->mfxVideoParams.mfx.RateControlMethod != MFX_RATECONTROL_CQP) ?
                                                            par->mfxVideoParams.mfx.TargetKbps * multiplier : 0;
        m_sps.vbv_buffer_size =((par->mfxVideoParams.mfx.BufferSizeInKB * multiplier) >> 1);
        m_sps.low_delay = 0;
        m_sps.ChromaFormat = (UCHAR)par->mfxVideoParams.mfx.FrameInfo.ChromaFormat;
        m_sps.progressive_sequence = par->mfxVideoParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE? 1:0;
        m_sps.Profile = (UCHAR)par->mfxVideoParams.mfx.CodecProfile;
        m_sps.Level = (UCHAR)par->mfxVideoParams.mfx.CodecLevel;
        m_sps.TargetUsage = (UCHAR)par->mfxVideoParams.mfx.TargetUsage;        

        m_sps.RateControlMethod = bAllowBRC ? (UCHAR)par->mfxVideoParams.mfx.RateControlMethod: 0;
        m_sps.MaxBitRate        = (UINT)par->mfxVideoParams.mfx.MaxKbps * multiplier;
        m_sps.MinBitRate        = m_sps.bit_rate;
        m_sps.UserMaxFrameSize  = par->mfxVideoParams.mfx.BufferSizeInKB * 1000 * multiplier;
        m_sps.InitVBVBufferFullnessInBit = par->mfxVideoParams.mfx.InitialDelayInKB * 8000 * multiplier;
        m_sps.AVBRAccuracy = par->mfxVideoParams.mfx.Accuracy;
        m_sps.AVBRConvergence = par->mfxVideoParams.mfx.Convergence;
    }

    if (par->bMbqpMode)
    {
        mfxU32 wMB = (par->mfxVideoParams.mfx.FrameInfo.CropW + 15) / 16;
        mfxU32 hMB = (par->mfxVideoParams.mfx.FrameInfo.CropH + 15) / 16;
        m_mbqp_data = new mfxU8[wMB*hMB];
    }    

    // m_caps parameters

    m_caps.IntraPredBlockSize = (par->bAllowFieldDCT) ?
        ENC_INTER_BLOCK_SIZE_16x16|ENC_INTER_BLOCK_SIZE_16x8:ENC_INTRA_BLOCK_16x16;
    m_caps.IntraPredCostType  = ENC_COST_TYPE_SAD; 
    m_caps.InterPredBlockSize = (par->bAllowFieldDCT) ? 
        ENC_INTER_BLOCK_SIZE_16x16|ENC_INTER_BLOCK_SIZE_16x8:
        ENC_INTER_BLOCK_SIZE_16x16;
  
    m_caps.MVPrecision           = ENC_MV_PRECISION_INTEGER|ENC_MV_PRECISION_HALFPEL;    
    m_caps.MECostType            = ENC_COST_TYPE_PROPRIETARY; 
    m_caps.MESearchType          = ENC_INTER_SEARCH_TYPE_PROPRIETARY; 
    m_caps.MVSearchWindowX       = par->MVRangeP[0];
    m_caps.MVSearchWindowY       = par->MVRangeP[1]; 
    m_caps.MEInterpolationMethod = ENC_INTERPOLATION_TYPE_BILINEAR;
    //m_caps.MEFractionalSearchType= ENC_COST_TYPE_SAD;
    m_caps.MaxMVs                = 4;
    m_caps.SkipCheck             = 1;
    m_caps.DirectCheck           = 1;
    m_caps.BiDirSearch           = (par->mfxVideoParams.mfx.GopRefDist > 1)? 1:0;
    m_caps.MBAFF                 = 0;
    m_caps.FieldPrediction       = (par->bFieldCoding && par->mfxVideoParams.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) ? 1:0;
    m_caps.RefOppositeField      = 0;
    m_caps.ChromaInME            = 0;
    m_caps.WeightedPrediction    = 0;
    m_caps.RateDistortionOpt     = 0;
    m_caps.MVPrediction          = 1;  
    m_caps.DirectVME             = 0;
    
    InitFramesSet(0, 0, 0, 0, 0);
    

#ifdef MFX_UNDOCUMENTED_QUANT_MATRIX
    SetQMParameters(&par->sQuantMatrix,m_quantMatrix);
#endif

    m_encrypt.Init(par, funcId);



    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::Init(const mfxVideoParamEx_MPEG2* par)


mfxStatus ExecuteBuffers::Close()
{
    delete [] m_pSlice;
    m_pSlice = 0;

    m_nSlices = 0;

    memset(&m_caps, 0, sizeof(m_caps));
    memset(&m_sps,  0, sizeof(m_sps));
    memset(&m_pps,  0, sizeof(m_pps));
    
    m_idxMb = (DWORD(-1));
    delete [] m_pMBs;
    delete [] m_mbqp_data;
    m_mbqp_data = 0;

    m_nMBs = 0;
    m_pMBs  = 0;

    if (m_bOutOfRangeMV)
    {
        //printf ("\n\n --------- ERROR: Out of range MVs are found out. ---------\n\n");
        m_bOutOfRangeMV = false;
    }
    if (m_bErrMBType)
    {
        //printf ("\n\n --------- ERROR: incorrect MB type ---------\n\n");
        m_bOutOfRangeMV = false;
    }
    
    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::Close()


mfxStatus ExecuteBuffers::InitPictureParameters(mfxFrameParamMPEG2*  pParams, mfxU32 frameNum)
{
    m_pps.CurrReconstructedPic.bPicEntry = 0;
    m_pps.RefFrameList[0].bPicEntry      = 0;
    m_pps.RefFrameList[1].bPicEntry      = 0;
    m_pps.temporal_reference             = pParams->TemporalReference;

    if (pParams->FrameType & MFX_FRAMETYPE_I)
    {
        m_pps.picture_coding_type = CODING_TYPE_I;
    }
    else if (pParams->FrameType & MFX_FRAMETYPE_P)
    {
        m_pps.picture_coding_type = CODING_TYPE_P;
    }
    else if (pParams->FrameType & MFX_FRAMETYPE_B)
    {
        m_pps.picture_coding_type = CODING_TYPE_B;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
    m_pps.FieldCodingFlag = 0;
    m_pps.FieldFrameCodingFlag = 0;
    m_pps.InterleavedFieldBFF = 0;
    m_pps.ProgressiveField = 0;
    m_pps.CurrReconstructedPic.AssociatedFlag = 0;

    if (pParams->ProgressiveFrame)
    {
        m_pps.FieldCodingFlag        = 0;
        m_pps.FieldFrameCodingFlag    = 0;
        m_pps.InterleavedFieldBFF    = (pParams->TopFieldFirst)? 0:1;
    }
    else if (pParams->FieldPicFlag)
    {
        m_pps.FieldCodingFlag        = 1;
        m_pps.FieldFrameCodingFlag    = 0;
        m_pps.InterleavedFieldBFF    = 0;
        m_pps.CurrReconstructedPic.AssociatedFlag = (pParams->BottomFieldFlag)? 1:0;
    }
    else
    {
        m_pps.FieldCodingFlag         = 0;
        m_pps.FieldFrameCodingFlag    = 1;
        m_pps.InterleavedFieldBFF     = (pParams->TopFieldFirst)? 0:1;
    }
    
    m_pps.NumSlice                   =  UCHAR(pParams->FrameHinMbMinus1 + 1);
    m_pps.bPicBackwardPrediction     =  pParams->BackwardPredFlag;
    m_pps.bBidirectionalAveragingMode= (pParams->BackwardPredFlag && pParams->ForwardPredFlag); 
    m_pps.bUseRawPicForRef           =  m_bUseRawFrames;
    m_pps.StatusReportFeedbackNumber =  frameNum + 1;

    m_pps.alternate_scan             = pParams->AlternateScan;
    m_pps.intra_vlc_format           = pParams->IntraVLCformat;
    m_pps.q_scale_type               = pParams->QuantScaleType;
    m_pps.concealment_motion_vectors = pParams->ConcealmentMVs;
    m_pps.frame_pred_frame_dct       = (pParams->FrameDCTprediction )?1:0;


    m_pps.DisableMismatchControl     = 0;
    m_pps.intra_dc_precision         = pParams->IntraDCprecision;

    m_pps.f_code00                   = (pParams->BitStreamFcodes >> 12) & 0x0f;
    m_pps.f_code01                   = (pParams->BitStreamFcodes >> 8 ) & 0x0f;
    m_pps.f_code10                   = (pParams->BitStreamFcodes >> 4 ) & 0x0f;
    m_pps.f_code11                   = (pParams->BitStreamFcodes >> 0 ) & 0x0f;

    
    m_pps.bLastPicInStream           = (pParams->ExtraFlags & MFX_IFLAG_ADD_EOS)!=0 ? 1:0; 
    m_pps.bNewGop                    = m_pps.picture_coding_type == CODING_TYPE_I && (!pParams->FieldPicFlag || pParams->SecondFieldFlag)? 1:0;
    m_pps.GopPicSize                 = m_GOPPictureSize;
    m_pps.GopRefDist                 = m_GOPRefDist;
    m_pps.GopOptFlag                 = m_GOPOptFlag;
    {
        mfxI32 num = 0;
        mfxI32 fps = 0, pict = 0, sec = 0, minute = 0, hour = 0;
        num = frameNum;        
        fps = (Ipp32s)(m_fFrameRate + 0.5);
        pict = num % fps;
        num = (num - pict) / fps;
        sec = num % 60;
        num = (num - sec) / 60;
        minute = num % 60;
        num = (num - minute) / 60;
        hour = num % 24;
        m_pps.time_code =(hour<<19) | (minute<<13) | (1<<12) | (sec<<6) | pict;
    }
    m_pps.temporal_reference        =   pParams->TemporalReference;
    m_pps.vbv_delay                 =   (USHORT)pParams->VBVDelay;
    m_pps.repeat_first_field        =   pParams->RepeatFirstField;
    m_pps.composite_display_flag    =   0;
    m_pps.v_axis                    =   0;
    m_pps.field_sequence            =   0;
    m_pps.sub_carrier               =   0;
    m_pps.burst_amplitude           =   0;
    m_pps.sub_carrier_phase         =   0;

    if ((pParams->ExtraFlags & MFX_IFLAG_ADD_HEADER)!=0 && !pParams->SecondFieldFlag)
    {
        m_bAddSPS = 1;
    }
    m_pps.bPic4MVallowed = 1;

    return MFX_ERR_NONE;
} 
void ExecuteBuffers::InitFramesSet(mfxMemId curr, bool bExternal, mfxMemId rec, mfxMemId ref_0,mfxMemId ref_1)
{

    m_CurrFrameMemID        = curr;
    m_bExternalCurrFrame    = bExternal;
    m_RecFrameMemID         = rec;
    m_RefFrameMemID[0]      = ref_0;
    m_RefFrameMemID[1]      = ref_1;

}
mfxStatus ExecuteBuffers::InitPictureParameters(mfxFrameCUC* pCUC)
{
    mfxStatus               sts = MFX_ERR_NONE;
    mfxFrameParamMPEG2*     pParams = &pCUC->FrameParam->MPEG2;
    
    sts = InitPictureParameters(pParams,pCUC->FrameSurface->Data[pCUC->FrameParam->MPEG2.CurrFrameLabel]->FrameOrder);
    MFX_CHECK_STS(sts);

    MFX_CHECK(pParams->CurrFrameLabel < pCUC->FrameSurface->NumFrameData, MFX_ERR_NOT_FOUND);
    m_CurrFrameMemID = pCUC->FrameSurface->Data[pParams->CurrFrameLabel]->MemId;
    m_bExternalCurrFrame = (pCUC->FrameSurface->Data[pParams->CurrFrameLabel]->reserved[0] == 0) ? true : false;

    MFX_CHECK(pParams->RecFrameLabel < pCUC->FrameSurface->NumFrameData, MFX_ERR_NOT_FOUND);
    m_RecFrameMemID = pCUC->FrameSurface->Data[pParams->RecFrameLabel]->MemId;

    if (m_pps.picture_coding_type == CODING_TYPE_P)
    {
         MFX_CHECK(pParams->RefFrameListP[0] < pCUC->FrameSurface->NumFrameData, MFX_ERR_NOT_FOUND);
         m_RefFrameMemID[0]= pCUC->FrameSurface->Data[2]->MemId;
    }
    if (m_pps.picture_coding_type == CODING_TYPE_B)
    {
        if (pParams->ForwardPredFlag)
        {
            MFX_CHECK(pParams->RefFrameListB[0][0] < pCUC->FrameSurface->NumFrameData, MFX_ERR_NOT_FOUND);
            m_RefFrameMemID[0] = pCUC->FrameSurface->Data[2]->MemId;
        }

        if (pParams->BackwardPredFlag)
        {
            MFX_CHECK(pParams->RefFrameListB[1][0] < pCUC->FrameSurface->NumFrameData, MFX_ERR_NOT_FOUND);
            m_RefFrameMemID[1]= pCUC->FrameSurface->Data[3]->MemId;        
        }    
    }

    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::InitPictureParameters(mfxFrameCUC* pCUC)

mfxStatus ExecuteBuffers::InitSliceParameters(mfxFrameCUC* pCUC)
{
    if (pCUC->NumSlice > m_nSlices)
        return MFX_ERR_UNSUPPORTED;
    
    mfxU8  intra = (pCUC->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I)? 1:0;
    mfxI32 numMBs = 0;

    for (int i=0; i<(int)pCUC->NumSlice; i++)
    {
        ENCODE_SET_SLICE_HEADER_MPEG2*  pDDISlice = &m_pSlice[i];
        mfxSliceParamMPEG2*             pMFXSlice = &pCUC->SliceParam[i].MPEG2;

        pDDISlice->FirstMbX                = pMFXSlice->FirstMbX;
        pDDISlice->FirstMbY                = pMFXSlice->FirstMbY;
        pDDISlice->NumMbsForSlice        = pMFXSlice->NumMb;
        pDDISlice->IntraSlice            = intra; 
        pDDISlice->quantiser_scale_code = pCUC->MbParam->Mb[numMBs].MPEG2.QpScaleCode;
        
        numMBs += pMFXSlice->NumMb;
    }

    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::InitSliceParameters(mfxFrameCUC* pCUC)


enum
{
    MBTYPE_INTRA                = 0x1A,
    MBTYPE_FORWARD              = 0x01,
    MBTYPE_BACKWARD             = 0x02,
    MBTYPE_BI                   = 0x03,
    
    MBTYPE_DUALPRIME            = 0x19,
    MBTYPE_FIELD_PRED_FORW      = 0x04,
    MBTYPE_FIELD_PRED_BACW      = 0x06,    
    MBTYPE_FIELD_PRED_BI        = 0x14,    
};

#define MB_MV(pmb,dir,num) (pmb)->MV[(dir)*4+(num)]
#define MB_FSEL(pmb,dir,i) ((mfxMbCodeAVC*)pmb)->RefPicSelect[dir][i] // tmp cast

#define _TEMP_DEBUG


static Ipp32s QuantToScaleCode(Ipp32s quant_value, Ipp32s q_scale_type)
{
    if (q_scale_type == 0)
    {
        return (quant_value + 1)/2;
    }
    else
    {
        if (quant_value <= 8)
            return quant_value;
        else if (quant_value <= 24)
            return 8 + (quant_value - 8)/2;
        else if (quant_value <= 56)
            return 16 + (quant_value - 24)/4;
        else 
            return 24 + (quant_value - 56)/8;
    }
}

mfxStatus ExecuteBuffers::InitSliceParameters(mfxU8 qp, mfxU16 scale_type, mfxU8 * mbqp, mfxU32 numMB)
{
    if (m_pps.NumSlice > m_nSlices)
        return MFX_ERR_UNSUPPORTED;
    
    mfxU8  intra = (m_pps.picture_coding_type == CODING_TYPE_I)? 1:0;
    mfxU16 numMBSlice = (mfxU16)((m_sps.FrameWidth +15)>>4);

    bool isMBQP = (m_mbqp_data != 0) && (mbqp != 0) && (mbqp[0] != 0);

    if (isMBQP)
    {
        for (mfxU32 i = 0; i < numMB; i++)
        {
            m_mbqp_data[i] = (Ipp8u)QuantToScaleCode(mbqp[i], scale_type);
        }
    }
   
    for (int i=0; i<(int)m_pps.NumSlice; i++)
    {
        ENCODE_SET_SLICE_HEADER_MPEG2*  pDDISlice = &m_pSlice[i];
        pDDISlice->FirstMbX                       = 0;
        pDDISlice->FirstMbY                       = (mfxU16)i;
        pDDISlice->NumMbsForSlice                 = numMBSlice;
        pDDISlice->IntraSlice                     = intra; 
        pDDISlice->quantiser_scale_code           = isMBQP ? mbqp[i*numMBSlice] : qp;
        //pDDISlice->quantiser_scale_code           = qp;
    }

    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::InitSliceParameters(mfxFrameCUC* pCUC)

mfxStatus ExecuteBuffers::SetMBParameters(mfxFrameCUC* pCUC)
{
    for (int i=0; i<(int)pCUC->NumMb;i++)
    {        
        ENCODE_ENC_MB_DATA_MPEG2* pMB = &m_pMBs[i];
        mfxMbCodeMPEG2 *pMFXMB = &pCUC->MbParam->Mb[i].MPEG2;
        memset(pMB,0,sizeof(ENCODE_ENC_MB_DATA_MPEG2));        
        pMB->QpScaleCode = pMFXMB->QpScaleCode;

        pMB->MbX = pMFXMB->MbXcnt;
        pMB->MbY = pMFXMB->MbYcnt;
    }

    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::SetMBParameters(mfxFrameCUC* pCUC)

bool CheckMV (ENCODE_ENC_MB_DATA_MPEG2* pMB, ENCODE_SET_PICTURE_PARAMETERS_MPEG2 *pPic, ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2 *pSeq)
{
    // all calculations in half pixels
    static int ranges[16] = {1,8<<1,8<<2,8<<3,8<<4,8<<5,8<<6,8<<7,8<<8,8<<9,1,1,1,1,1,1};
    static int ranges_v[16] = {1,4<<1,4<<2,4<<3,4<<4,4<<5,4<<6,4<<7,4<<8,4<<9,1,1,1,1,1,1};

    int *ranges_y = (pPic->FieldFrameCodingFlag && pMB->FieldMBFlag)? ranges_v:ranges;

    int posX        = pMB->MbX*16*2;
    int posY0       = pMB->MbY*16*2;

    int width  = pSeq->FrameWidth*2;
    int height = pSeq->FrameHeight*2;
    int shift = 0;

    if (pPic->FieldCodingFlag)
    {
        height = height/2; //we are predicted from field
    }
    if (pPic->FieldFrameCodingFlag && pMB->FieldMBFlag)
    {
        height = height/2; //we are predicted from field
        posY0 = posY0/2;
        height -= 8*2; //space for last MB
        shift   = 1;
    }
    else
    {
        height -= 16*2;//space for last MB
    } 

    // out of picture

    if (posX + pMB->MVL0[0].x < 0 || posX + pMB->MVL0[0].x > width)
    {
        return false;
    }
    if (posX + pMB->MVL1[0].x < 0 || posX + pMB->MVL1[0].x > width)
    {
        return false;
    }
    if (posX + pMB->MVL0[1].x < 0 || posX + pMB->MVL0[1].x > width)
    {
        return false;
    }
    if (posX + pMB->MVL1[1].x < 0 || posX + pMB->MVL1[1].x > width)
    {
        return false;
    }

    if (posY0 + (pMB->MVL0[0].y>>shift) < 0 || posY0 + (pMB->MVL0[0].y>>shift) > height)
    {
        return false;
    }
    if (posY0 + (pMB->MVL1[0].y>>shift) < 0 || posY0 + (pMB->MVL1[0].y>>shift) > height)
    {
        return false;
    }
    if (posY0 + (pMB->MVL0[1].y>>shift) < 0 || posY0 + (pMB->MVL0[1].y>>shift) > height)
    {
        return false;
    }
    if (posY0 + (pMB->MVL1[1].y>>shift) < 0 || posY0 + (pMB->MVL1[1].y>>shift) > height)
    {
        return false;
    }

    // f_codes

    if ( pMB->MVL0[0].x < (-1)*ranges[pPic->f_code00] ||  pMB->MVL0[0].x > ranges[pPic->f_code00] - 1)
    {
         return false;
    }

    if ( pMB->MVL0[1].x < (-1)*ranges[pPic->f_code10] ||  pMB->MVL0[1].x > ranges[pPic->f_code10] - 1)
    {
         return false;
    }
    if ( pMB->MVL1[0].x < (-1)*ranges[pPic->f_code00] ||  pMB->MVL1[0].x > ranges[pPic->f_code00] - 1)
    {
         return false;
    }

    if ( pMB->MVL1[1].x < (-1)*ranges[pPic->f_code10] ||  pMB->MVL1[1].x > ranges[pPic->f_code10] - 1)
    {
         return false;
    }


    if ( pMB->MVL0[0].y < (-1)*ranges_y[pPic->f_code01] ||  pMB->MVL0[0].y > ranges_y[pPic->f_code01] - 1)
    {
         return false;
    }

    if ( pMB->MVL0[1].y < (-1)*ranges_y[pPic->f_code11] ||  pMB->MVL0[1].y > ranges_y[pPic->f_code11] - 1)
    {
         return false;
    }
    if ( pMB->MVL1[0].y < (-1)*ranges_y[pPic->f_code01] ||  pMB->MVL1[0].y > ranges_y[pPic->f_code01] - 1)
    {
         return false;
    }

    if ( pMB->MVL1[1].y < (-1)*ranges_y[pPic->f_code11] ||  pMB->MVL1[1].y > ranges_y[pPic->f_code11] - 1)
    {
         return false;
    }

    return true;

} // bool CheckMV (ENCODE_ENC_MB_DATA_MPEG2* pMB, ENCODE_SET_PICTURE_PARAMETERS_MPEG2 *pPic, ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2 *pSeq)


mfxStatus ExecuteBuffers::GetMBParameters(mfxFrameCUC* pCUC)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ConvertMB2CUC");
    //ENCODE_SET_SLICE_HEADER_MPEG2*  pDDISlice = m_pSlice;
    
    USHORT numMBPerSlice = 0;
    USHORT numSlice = 0;

    //FILE* f = fopen ("ENC_OUTPUT.txt","a+");
    //fprintf (f,"new frame (m_pps.f_code00 - %d, 01 - %d, 10 - %d, 00 - %d)\n",m_pps.f_code00,m_pps.f_code01,m_pps.f_code10,m_pps.f_code11);
    //fprintf(f,"numMB\ttype\tMVL0[0].x\tpMB->MVL0[0].y\tpMB->MVL0[1].x\tpMB->MVL0[1].y\n");

    if (pCUC->FrameParam->MPEG2.FrameType & MFX_FRAMETYPE_I)
    {
    
        for (int i=0; i<(int)pCUC->NumMb;i++)
        {
            mfxMbCodeMPEG2 *pMFXMB = &pCUC->MbParam->Mb[i].MPEG2;
            
            (MB_MV(pMFXMB,0,0))[0] = (MB_MV(pMFXMB,0,0))[1] =
            (MB_MV(pMFXMB,0,1))[0] = (MB_MV(pMFXMB,0,1))[1] =
            (MB_MV(pMFXMB,1,0))[0] = (MB_MV(pMFXMB,1,0))[1] = 
            (MB_MV(pMFXMB,1,1))[0] = (MB_MV(pMFXMB,1,1))[1] = 0;

            MB_FSEL(pMFXMB,0,0) = MB_FSEL(pMFXMB,0,1) = 
            MB_FSEL(pMFXMB,1,0) = MB_FSEL(pMFXMB,1,1) = 0; 

            pMFXMB->MbType5Bits   = MBTYPE_INTRA;
            pMFXMB->IntraMbFlag   = 1;    

            pMFXMB->FieldMbFlag   = 0;
            pMFXMB->TransformFlag = 0;
            pMFXMB->Skip8x8Flag   = 0;
            pMFXMB->MbXcnt        = (mfxU8)numMBPerSlice;
            pMFXMB->MbYcnt        = (mfxU8)numSlice;

            if (numMBPerSlice < m_pSlice[numSlice].NumMbsForSlice-1)
            {
                numMBPerSlice ++;
            }
            else 
            {
                numMBPerSlice = 0;
                numSlice ++;        
            }
            pMFXMB->CodedPattern4x4Y = 0xffff;        
            pMFXMB->CodedPattern4x4U = 0xffff;
            pMFXMB->CodedPattern4x4V = 0xffff;

            // pMFXMB->QpScaleCode was defined in encode.  

        }        
    }
    else
    {
        for (int i=0; i<(int)pCUC->NumMb;i++)
        {
            mfxMbCodeMPEG2 *pMFXMB = &pCUC->MbParam->Mb[i].MPEG2;
            ENCODE_ENC_MB_DATA_MPEG2* pMB = &m_pMBs[i];
            if (!CheckMV (pMB, &m_pps, &m_sps))
            {
                (MB_MV(pMFXMB,0,0))[0] = 0;
                (MB_MV(pMFXMB,0,0))[1] = 0;

                (MB_MV(pMFXMB,0,1))[0] = 0;
                (MB_MV(pMFXMB,0,1))[1] = 0;

                (MB_MV(pMFXMB,1,0))[0] = 0;
                (MB_MV(pMFXMB,1,0))[1] = 0;

                (MB_MV(pMFXMB,1,1))[0] = 0;
                (MB_MV(pMFXMB,1,1))[1] = 0;  

                m_bOutOfRangeMV = true;
            }
            else
            {            
                (MB_MV(pMFXMB,0,0))[0] = pMB->MVL0[0].x;
                (MB_MV(pMFXMB,0,0))[1] = pMB->MVL0[0].y;

                (MB_MV(pMFXMB,1,0))[0] = pMB->MVL0[1].x;
                (MB_MV(pMFXMB,1,0))[1] = pMB->MVL0[1].y;

                (MB_MV(pMFXMB,0,1))[0] = pMB->MVL1[0].x;
                (MB_MV(pMFXMB,0,1))[1] = pMB->MVL1[0].y;

                (MB_MV(pMFXMB,1,1))[0] = pMB->MVL1[1].x;
                (MB_MV(pMFXMB,1,1))[1] = pMB->MVL1[1].y;
            }

            MB_FSEL(pMFXMB,0,0) = (mfxU8)((pMB->MvFieldSelect)    & 0x01);
            MB_FSEL(pMFXMB,1,0) = (mfxU8)((pMB->MvFieldSelect>>1) & 0x01);
            MB_FSEL(pMFXMB,0,1) = (mfxU8)((pMB->MvFieldSelect>>2) & 0x01);
            MB_FSEL(pMFXMB,1,1) = (mfxU8)((pMB->MvFieldSelect>>3) & 0x01); 

            //fprintf(f,"%d\t%d\t%d\t%d\t%d\t%d\n",i,pMB->macroblock_type,pMB->MVL0[0].x,pMB->MVL0[0].y,pMB->MVL0[1].x,pMB->MVL0[1].y);
        
            switch (pMB->macroblock_type)
            {
            case MBTYPE_INTRA:
                MFX_CHECK(pMB->macroblock_intra, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->MbType5Bits   = pMB->macroblock_type;
                pMFXMB->IntraMbFlag   = 1;
                pMFXMB->NumPackedMv   = 0;
                break;

            case MBTYPE_FORWARD:
            case MBTYPE_BACKWARD:
                MFX_CHECK(!pMB->macroblock_intra, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->MbType5Bits   = pMB->macroblock_type;
                pMFXMB->IntraMbFlag   = 0;
                pMFXMB->NumPackedMv   = 1;
                break;

            case MBTYPE_BI:
                MFX_CHECK(!pMB->macroblock_intra, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->MbType5Bits   = pMB->macroblock_type;
                pMFXMB->IntraMbFlag   = 0;
                pMFXMB->NumPackedMv   = 2;
                break;

            case MBTYPE_DUALPRIME:
                //printf("Error ME: dual prime is not supported!!!!\n");
                MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

            case MBTYPE_FIELD_PRED_FORW:
            case MBTYPE_FIELD_PRED_BACW:
                MFX_CHECK(!pMB->macroblock_intra, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->MbType5Bits   = pMB->macroblock_type;
                pMFXMB->IntraMbFlag   = 0;
                pMFXMB->NumPackedMv   = 2;
            case MBTYPE_FIELD_PRED_BI:
                MFX_CHECK(!pMB->macroblock_intra, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->MbType5Bits   = pMB->macroblock_type;
                pMFXMB->IntraMbFlag   = 0;
                pMFXMB->NumPackedMv   = 4;
                break;

            default:
                //printf("Error ME: pMB->macroblock_type is not correct (%d) - MB = %d\n",pMB->macroblock_type,i);
                // MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
                m_bErrMBType = true;
                pMB->macroblock_intra = 1;
                pMFXMB->MbType5Bits   = MBTYPE_INTRA;
                pMFXMB->IntraMbFlag   = 1;
                pMFXMB->NumPackedMv   = 0;
                break;                
            }

            if ( m_pps.FieldCodingFlag )
            {
                pMFXMB->FieldMbFlag   = 1;
                pMFXMB->TransformFlag = 0;
            }
            else if (m_pps.FieldFrameCodingFlag)
            {
                pMFXMB->FieldMbFlag   = pMB->FieldMBFlag;
                pMFXMB->TransformFlag = pMB->dct_type;             
            }
            else
            {   
                MFX_CHECK(!pMB->FieldMBFlag, MFX_ERR_UNDEFINED_BEHAVIOR);
                MFX_CHECK(!pMB->dct_type, MFX_ERR_UNDEFINED_BEHAVIOR);
                pMFXMB->FieldMbFlag   = 0;
                pMFXMB->TransformFlag = 0;
            }

            MFX_CHECK(pMB->MbX == numMBPerSlice, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(pMB->MbY == numSlice,      MFX_ERR_UNDEFINED_BEHAVIOR);
            pMFXMB->MbXcnt        = mfxU8(pMB->MbX);
            pMFXMB->MbYcnt        = mfxU8(pMB->MbY);

            pMFXMB->Skip8x8Flag = 0;

    #ifndef _TEMP_DEBUG
            pMFXMB->CodedPattern4x4Y  = ((pMB->coded_block_pattern>>11)&0x01) ? 0x000f : 0;
            pMFXMB->CodedPattern4x4Y |= ((pMB->coded_block_pattern>>10)&0x01) ? 0x00f0 : 0;
            pMFXMB->CodedPattern4x4Y |= ((pMB->coded_block_pattern>> 9)&0x01) ? 0x0f00 : 0;
            pMFXMB->CodedPattern4x4Y |= ((pMB->coded_block_pattern>> 8)&0x01) ? 0xf000 : 0;
            
            pMFXMB->CodedPattern4x4U = ((pMB->coded_block_pattern>> 7)&0x01) ? 0xffff : 0;
            pMFXMB->CodedPattern4x4V = ((pMB->coded_block_pattern>> 6)&0x01) ? 0xffff : 0;
    #else
            pMFXMB->CodedPattern4x4Y  = 0xffff;        
            pMFXMB->CodedPattern4x4U = 0xffff;
            pMFXMB->CodedPattern4x4V = 0xffff;
    #endif 
    #ifndef _TEMP_DEBUG
            pMFXMB->QpScaleCode = pMB->QpScaleCode;
    #endif
            /*if (pMB->SkipMbFlag)
            {
                //skip mode doesn't work in the case of interlace fields:
                //if frame is used opposite filds for prediction, MB is not skipped
                pMFXMB->NumPackedMv = 0;
                pMFXMB->Skip8x8Flag = 1;
                pMFXMB->CodedPattern4x4Y = 0; 
                pMFXMB->CodedPattern4x4U = 0;
                pMFXMB->CodedPattern4x4V = 0;
            }*/
    

            if (numMBPerSlice < m_pSlice[numSlice].NumMbsForSlice-1)
            {
                numMBPerSlice ++;
            }
            else
            {
                numMBPerSlice = 0;
                numSlice ++;        
            }            
        }
    }
    //fclose(f);

    return MFX_ERR_NONE;

} // mfxStatus ExecuteBuffers::GetMBParameters(mfxFrameCUC* pCUC)

//--------AYA: fixme!!! depreciated functions
namespace MfxHwMpeg2Encode
{
    bool ConvertFrameRateMPEG2(mfxU32 FrameRateExtD, mfxU32 FrameRateExtN, mfxI32 &frame_rate_code, mfxI32 &frame_rate_extension_n, mfxI32 &frame_rate_extension_d)
    {
        static const mfxF64 ratetab[8]=
        {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};

        const mfxI32 sorted_ratio[][2] = 
        {
            {1,32},{1,31},{1,30},{1,29},{1,28},{1,27},{1,26},{1,25},{1,24},{1,23},{1,22},{1,21},{1,20},{1,19},{1,18},{1,17},
            {1,16},{2,31},{1,15},{2,29},{1,14},{2,27},{1,13},{2,25},{1,12},{2,23},{1,11},{3,32},{2,21},{3,31},{1,10},{3,29},
            {2,19},{3,28},{1, 9},{3,26},{2,17},{3,25},{1, 8},{4,31},{3,23},{2,15},{3,22},{4,29},{1, 7},{4,27},{3,20},{2,13},
            {3,19},{4,25},{1, 6},{4,23},{3,17},{2,11},{3,16},{4,21},{1, 5},{4,19},{3,14},{2, 9},{3,13},{4,17},{1, 4},{4,15},
            {3,11},{2, 7},{3,10},{4,13},{1, 3},{4,11},{3, 8},{2, 5},{3, 7},{4, 9},{1, 2},{4, 7},{3, 5},{2, 3},{3, 4},{4, 5},
            {1,1},{4,3},{3,2},{2,1},{3,1},{4,1}
        };

        const mfxI32 srsize = sizeof(sorted_ratio)/sizeof(sorted_ratio[0]);
        const mfxI32 rtsize = sizeof(ratetab)/sizeof(ratetab[0]);

        if (!FrameRateExtD || !FrameRateExtN)
        {
            return false;
        } 

        mfxF64    new_fr = (mfxF64)FrameRateExtN/(mfxF64)FrameRateExtD;
        mfxI32    i=0, j=0, besti=0, bestj=0;
        mfxF64    ratio=0.0, bestratio = 1.5;
        mfxI32    fr1001 = (Ipp32s)(new_fr*1001+.5);

        frame_rate_code = 5;       
        frame_rate_extension_n = 0;
        frame_rate_extension_d = 0;

        for(j=0;j<rtsize;j++) 
        {
            Ipp32s try1001 = (Ipp32s)(ratetab[j]*1001+.5);
            if(fr1001 == try1001) 
            {
                frame_rate_code = j+1;
                return true;
            }
        }
        if(new_fr < ratetab[0]/sorted_ratio[0][1]*0.7)
            return false;

        for(j=0;j<rtsize;j++) 
        {
            ratio = ratetab[j] - new_fr; // just difference here
            if(ratio < 0.0001 && ratio > -0.0001) 
            { // was checked above with bigger range
                frame_rate_code = j+1;
                frame_rate_extension_n = frame_rate_extension_d = 0;
                return true;
            }
            
            for(i=0;i<srsize+1;i++) 
            { // +1 because we want to analyze last point as well
                if((i<srsize)? (ratetab[j]*sorted_ratio[i][0] > new_fr*sorted_ratio[i][1]) : true) 
                {
                    if(i>0) 
                    {
                        ratio = ratetab[j]*sorted_ratio[i-1][0] / (new_fr*sorted_ratio[i-1][1]); // up to 1
                        if(1/ratio < bestratio) 
                        {
                            besti = i-1;
                            bestj = j;
                            bestratio = 1/ratio;
                        }
                    }
                    if(i<srsize) 
                    {
                        ratio = ratetab[j]*sorted_ratio[i][0] / (new_fr*sorted_ratio[i][1]); // down to 1
                        if(ratio < bestratio) 
                        {
                            besti = i;
                            bestj = j;
                            bestratio = ratio;
                        }
                    }
                    break;
                }
            }
        }

        if(bestratio > 1.005)
            return false;

        frame_rate_code = bestj+1;
        frame_rate_extension_n = sorted_ratio[besti][0]-1;
        frame_rate_extension_d = sorted_ratio[besti][1]-1;

        return true;

    }
}

#endif
#endif
/* EOF */
