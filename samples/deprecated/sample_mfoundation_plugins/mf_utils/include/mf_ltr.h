/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "mf_guids.h"
#include "mf_utils.h"
#include <bitset>
#include <map>
#include <set>

#include "mf_mfx_params_interface.h"

//CODECAPI_AVEncVideoLTRBufferControl:

//Note: GetParameterValues( ) supports at most 34 valid values.
//That is, the number of supported LTR frames can be 0 to 16, inclusive, under mode 0 and mode 1.
#define MF_LTR_BC_SIZE_MAX 16

#define MF_LTR_BC_MODE_TRUST_UNTIL      1

//CODECAPI_AVEncVideoUseLTRFrame
#define MF_LTR_USE_CONSTRAINTS_NO       0
#define MF_LTR_USE_CONSTRAINTS_YES      1
#define MF_LTR_USE_CONSTRAINTS_MAX      MF_LTR_USE_CONSTRAINTS_YES

#define MF_TL_LTR MF_TL_PERF

class FrameTypeGenerator
{
public:
    FrameTypeGenerator(void):
      m_frameOrder(0),
      m_gopOptFlag(0),
      m_gopPicSize(0),
      m_gopRefDist(0),
      m_cachedFrameType(MFX_FRAMETYPE_UNKNOWN),
      m_init()
      {
      }
    virtual ~FrameTypeGenerator(){}

    virtual void Init(mfxU16 GopOptFlag,
                      mfxU16 GopPicSize,
                      mfxU16 GopRefDist,
                      mfxU16 IdrInterval);

    virtual void ResetFrameOrder() { m_frameOrder = 0; m_cachedFrameType = MFX_FRAMETYPE_UNKNOWN; }

    virtual mfxU16 CurrentFrameType() const;
    virtual void Next();

private:

    mfxU32 m_frameOrder;    // in display order
    mfxU16 m_gopOptFlag;
    mfxU16 m_gopPicSize;
    mfxU16 m_gopRefDist;
    mfxU32 m_idrDist;
    bool m_init;
    //caches is initialized in const function but technically it wont change internal state
    mutable mfxU16 m_cachedFrameType;
};

/*
typedef struct {
    mfxExtBuffer    Header;
    mfxU16          NumRefIdxL0Active;
    mfxU16          NumRefIdxL1Active;

    struct {
        mfxU32      FrameOrder;
        mfxU16      PicStruct;
        mfxU16      ViewId;
        mfxU16      LongTermIdx;
        mfxU16      reserved[3];
    } PreferredRefList[32], RejectedRefList[16], LongTermRefList[16];

    mfxU16      ApplyLongTermIdx;.// 0 - don't use LongTermIdx, 1 - use LongTermIdx
    mfxU16      reserved[15];
} mfxExtAVCRefListCtrl;
*/

struct LongTermRefFrame
{
    mfxU32      FrameOrder;
    mfxU16      PicStruct;
    mfxU16      ViewId;
    mfxU16      LongTermIdx;
    mfxU16      reserved[3];
} ;

struct RefFrame
{
    RefFrame(mfxU32 F = MFX_FRAMEORDER_UNKNOWN, mfxU16 P = MFX_PICSTRUCT_UNKNOWN) :
    FrameOrder(F),
    PicStruct(P)
    {}

    mfxU32      FrameOrder;
    mfxU16      PicStruct;
};

typedef mfxU16 LongTermFrameIdx;
typedef std::map<LongTermFrameIdx, RefFrame>   RefList;


class MFLongTermReference : public MFParamsWorker
{
public:

    MFLongTermReference( MFParamsManager *pManager, FrameTypeGenerator *gen);
    virtual ~MFLongTermReference(void);

    //ICodecAPI wrappers:
    HRESULT     GetBufferControl(ULONG &nValue) const;
    HRESULT     SetBufferControl(ULONG nValue);

    HRESULT     MarkFrame(ULONG nLtrIndex);
    HRESULT     UseFrame(ULONG useLTRFramesMask);

    //ICodecAPI helpers
    bool        IsEnabled() const { return m_nBufferControlSize > 0; }
    UINT16      GetBufferControlSize() const { return m_nBufferControlSize; }
    UINT16      GetBufferControlMode() const { return m_nBufferControlMode; }

    //MFParamsWorker::
    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender);
    //virtual bool IsEnabled() const { return m_nTemporalLayersCount > 0; }
    //STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const;

    //Mfx params
    void        SetNumRefFrame(mfxU16 nNumRefFrame) { m_nNumRefFrame = nNumRefFrame; }
    void        SetGopParams(mfxU16 GopOptFlag,
                             mfxU16 GopPicSize,
                             mfxU16 GopRefDist,
                             mfxU16 IdrInterval)
                { m_frameTypeGen->Init(GopOptFlag, GopPicSize, GopRefDist, IdrInterval); }

    void        IncrementFrameOrder();
    void        ResetFrameOrder(void);
    void        ForceFrameType(mfxU16 nFrameType);

    // allocates array with one pointer to mfxExtAVCRefListCtrl
    // and updates given mfxEncodeCtrl with pointer to that array.
    // making the caller responsible for freeing all allocated memory
    // Requires empty extbuffer list on input
    HRESULT     GenerateFrameEncodeCtrlExtParam( mfxEncodeCtrl &encodeCtrl);

    UINT16                GetMaxBufferControlSize() const { return MF_LTR_BC_SIZE_MAX; }
protected: // functions

    HRESULT GenerateExtAVCRefList(mfxExtAVCRefListCtrl& extAvcRefListCtrl);

    void ExecuteUseCmd(std::vector<LongTermRefFrame> & preferredList, std::vector<LongTermRefFrame> & removeFromDPB);
    void UpdateLTRList(mfxU32 frameType, std::vector<LongTermRefFrame> & addToDPB, mfxU16 & idx);
    void UpdateSTRList(mfxU32 frameType, bool isLTR);

    mfxU16 FindMinFreeIdx(RefList const & refPicList) const;

    void PrintInfo() const;

protected: // variables
    std::auto_ptr<FrameTypeGenerator>    m_frameTypeGen;

    //CODECAPI_AVEncVideoLTRBufferControl
    UINT16                m_nBufferControlSize;
    UINT16                m_nBufferControlMode;

    //Mfx params
    mfxU16                m_nNumRefFrame;
    mfxU16                m_ForcedFrameType;

    mfxU32                m_nFrameOrder;
    mfxU32                m_prevFrameOrder;
    mfxExtAVCRefListCtrl   m_cachedExtAVCRefListCtrl;

    RefList                m_LTRFrames;
    std::set<mfxU32>       m_STRFrames;
    LongTermFrameIdx       m_MarkFrameIdx;     // CODECAPI_AVEncVideoMarkLTRFrame
    std::set<mfxU16>       m_preferredIdxs;
    bool                   m_bSaveConstraints;
    bool                   m_bToCleanSTRList;

    MFParamsManagerMessageType              m_tempLayerType; //information about current temporal layer
};

class MFLongTermReferenceFactory
{
public:
    static std::auto_ptr<MFLongTermReference> CreateLongTermReference(MFParamsManager *pManager);
};