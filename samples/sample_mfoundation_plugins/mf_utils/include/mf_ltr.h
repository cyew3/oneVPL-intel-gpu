/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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