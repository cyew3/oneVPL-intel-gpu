/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_encoder.h"
#include "ts_vpp.h"
#include "ts_decoder.h"

class tsTranscoder : public tsVideoVPP, public tsVideoDecoder, public tsVideoEncoder
{
public:
    struct SP
    {
        mfxSyncPoint sp;
        mfxBitstream bs;
    };
    tsExtBufType<mfxVideoParam>& m_parDEC;
    tsExtBufType<mfxVideoParam>& m_parVPP;
    tsExtBufType<mfxVideoParam>& m_parENC;
    tsBitstreamProcessor*&       m_bsProcIn;
    tsBitstreamProcessor*&       m_bsProcOut;
    mfxU16                       m_cId; // 0=dec, 1=vpp, 2=enc
    std::list<SP>                m_sp_free;
    std::list<SP>                m_sp_working;

    tsTranscoder(mfxU32 codecDec, mfxU32 codecEnc);

    ~tsTranscoder();

    void InitAlloc();

    void AllocSurfaces();

    void InitPipeline(mfxU16 AsyncDepth = 5);

    bool SubmitDecoding();

    bool SubmitVPP();

    bool SubmitEncoding();

    void TranscodeFrames(mfxU32 n);

    mfxFrameSurface1* GetSurface();
};