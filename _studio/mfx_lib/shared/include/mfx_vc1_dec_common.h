//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"


#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef _MFX_VC1_DEC_COMMON_H_
#define _MFX_VC1_DEC_COMMON_H_

#include "mfxvideo.h"
#include "umc_video_decoder.h"
#include "umc_vc1_common_defs.h"

namespace MFXVC1DecCommon
{
    typedef enum
    {
        IQT,
        PredDec,
        GetRecon,
        RunILDBL,
        Full
    } VC1DecEntryPoints;

    typedef enum
    {
        EndOfSequence              = 0x0A010000,
        Slice                      = 0x0B010000,
        Field                      = 0x0C010000,
        FrameHeader                = 0x0D010000,
        EntryPointHeader           = 0x0E010000,
        SequenceHeader             = 0x0F010000,
        SliceLevelUserData         = 0x1B010000,
        FieldLevelUserData         = 0x1C010000,
        FrameLevelUserData         = 0x1D010000,
        EntryPointLevelUserData    = 0x1E010000,
        SequenceLevelUserData      = 0x1F010000,

    } VC1StartCodeSwapped;

    // need for dword alignment memory
    inline mfxU32    GetDWord_s(mfxU8* pData) { return ((*(pData+3))<<24) + ((*(pData+2))<<16) + ((*(pData+1))<<8) + *(pData);}
    
    // we sure about our types
    template <class T, class T1>
    static T bit_set(T value, Ipp32u offset, Ipp32u count, T1 in)
    {
        return (T)(value | (((1<<count) - 1)&in) << offset);
    };

    mfxStatus ConvertMfxToCodecParams(mfxVideoParam *par, UMC::BaseCodecParams* pVideoParams);
    mfxStatus ParseSeqHeader(mfxBitstream *bs, mfxVideoParam *par, mfxExtVideoSignalInfo *pVideoSignal, mfxExtCodingOptionSPSPPS *pSPS);
    mfxStatus PrepareSeqHeader(mfxBitstream *bs_in, mfxBitstream *bs_out);

    mfxStatus Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out);
}

#endif
#endif
