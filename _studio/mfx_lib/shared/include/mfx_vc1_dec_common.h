/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"


#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef _MFX_VC1_DEC_COMMON_H_
#define _MFX_VC1_DEC_COMMON_H_

#include "mfxvideo.h"
#include "umc_video_decoder.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_job.h"


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

    mfxStatus FillmfxVideoParams(VC1Context* pContext, mfxVideoParam* pVideoParams);
    mfxStatus FillmfxPictureParams(VC1Context* pContext, mfxFrameParam* pFrameParam);
    
    mfxStatus Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out);


}

class MfxVC1ThreadUnitParams
{
public:
    Ipp16u                   MBStartRow;
    Ipp16u                   MBEndRow;
    Ipp16u                   MBRowsToDecode;
    bool                     isFirstInSlice;
    bool                     isLastInSlice;
    bool                     isFullMode;
    VC1Context*              pContext;
    mfxFrameCUC*             pCUC;
};
// base Task
class VC1TaskMfxBase //similar to VC1Task. Own processing Context
{
public:
    VC1TaskMfxBase();
    virtual ~VC1TaskMfxBase();

    mfxStatus Init(MfxVC1ThreadUnitParams*  pUnitParams,
                   MfxVC1ThreadUnitParams*  pPrevUnitParams,
                   Ipp32s                   threadOwn,
                   bool                     isReadyToProcess);

    bool    IsReadyToWork()  {return m_bIsReady;}
    bool    IsDone()         {return m_bIsDone;}
    Ipp32s  GetThreadOwner() {return m_ThreadOwner;}
    bool    IsProcess()      {return m_bIsProcess;}
    void    SetTaskAsDone()  {m_bIsDone = true;}
    mfxStatus virtual ProcessJob() = 0;

protected:
    Ipp16u                   m_MBStartRow;
    Ipp16u                   m_MBEndRow;
    Ipp16u                   m_MBRowsToDecode;
    bool                     m_bIsFirstInSlice;
    bool                     m_bIsLastInSlice;
    bool                     m_bIsReady;
    bool                     m_bIsProcess;
    bool                     m_bIsDone;
    bool                     m_bIsFullMode;
    Ipp32s                   m_ThreadOwner;
    VC1SingletonMB           m_SingleMB;

    VC1Context*              m_pContext;
    mfxFrameCUC*             m_pCUC;
};


//queue of tasks
class VC1MfxTQueueBase
{
public:
    VC1MfxTQueueBase():m_iTasksInQueue(0),
                       m_iCurrentTaskID(0)
    {
    };
    virtual ~VC1MfxTQueueBase()
    {
    };
    virtual mfxStatus Init() = 0;
    virtual VC1TaskMfxBase* GetNextStaticTask(Ipp32s threadID) = 0;
    virtual VC1TaskMfxBase* GetNextDynamicTask(Ipp32s threadID) = 0;

    virtual bool IsFuncComplte() = 0;

protected:
    Ipp32s m_iTasksInQueue;
    Ipp32s m_iCurrentTaskID;
};

//task processor
class MfxVC1TaskProcessor:  public UMC::VC1TaskProcessor
{
public:
    MfxVC1TaskProcessor(VC1MfxTQueueBase* pStore):m_pStore(pStore)
    {
    };
    virtual ~MfxVC1TaskProcessor()
    {
    };
    virtual UMC::Status process();
protected:
    VC1MfxTQueueBase* m_pStore;

};

#endif
#endif
