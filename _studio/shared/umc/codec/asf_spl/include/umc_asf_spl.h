//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_ASF_SPL_H__
#define __UMC_ASF_SPL_H__

#include "umc_splitter.h"
#include "umc_asf_parser.h"
#include "umc_data_reader.h"
#include "umc_asf_fb.h"
#include "umc_thread.h"


#define ASF_NUMBER_OF_FRAMES    1
#define ASF_PREF_FRAME_SIZE     4 * 1024 * 1024

namespace UMC
{

  class ASFSplitter : public Splitter
{

    DYNAMIC_CAST_DECL(ASFSplitter, Splitter)

public:

    ASFSplitter();
    ~ASFSplitter();

    Status  Init(SplitterParams& init);
    Status  Close();

    Status Run();
    Status Stop() {return UMC_OK;}  /* !!! DEPRECATED !!! */

    Status GetNextData(MediaData* data, Ipp32u nTrack);
    Status CheckNextData(MediaData* data, Ipp32u nTrack);

    Status SetTimePosition(Ipp64f /*position*/); // in second
    Status GetTimePosition(Ipp64f& /*position*/); // return current position in second
    Status SetRate(Ipp64f /*rate*/);

    Status GetInfo(SplitterInfo** ppInfo);
    Status EnableTrack(Ipp32u /*nTrack*/, Ipp32s /*iState*/);

protected:

    Status    GetESFromPID(Ipp32u &iES, Ipp32u streamNumber);
    Status    ReadNextObject(asf_Object *pObj);
    Status    ReadHeaderObject();
    Status    ReadFPropObject();
    Status    ReadStreamPropObject(Ipp32u nStream);
    Status    ReadExtStreamPropObject(asf_ExtStreamPropObject *pObj);
    Status    ReadHeaderExtObject();
    Status    ReadHeaderExtData(Ipp32u dataSize);
    Status    ReadCodecListObject();
    Status    ReadLangListObject();
    Status    ReadMetadataObject();
    Status    ReadPaddingObject();
    Status    ReadMutualExclObject();
    Status    ReadStrPrioritObject();
    Status    ReadIndxParamsObject();
    Status    ReadStreamBitratePropObject();
    Status    ReadBitrateMutualExclObject();
    Status    ReadErrCorrectObject();
    Status    ReadDataPacket();
    Status    FillVideoMediaInfo(asf_VideoMediaInfo *pVideoSpecData);
    Status    FillAudioMediaInfo(asf_AudioMediaInfo *pAudioSpecData);
    Status    FillSpreadAudioData(asf_SpreadAudioData *pSpreadAudioData);
    Status    ReadDataObject();
    Status    CountNumberOfStreams(Ipp32u &numOfStreams);
    Status    ReadDataFromType(Ipp8u val, Ipp32u &dst, Ipp32u &size);
    Status    FillAudioInfo(Ipp32u nTrack);
    Status    FillVideoInfo(Ipp32u nTrack);
    Status    CleanInternalObjects();
    Status    CleanHeaderObject();

    static Ipp32u VM_THREAD_CALLCONVENTION ReadDataPacketThreadCallback(void* pParam);

protected:

    asf_HeaderObject    *m_pHeaderObject;
    asf_DataObject      *m_pDataObject;
    ASFFrameBuffer      **m_ppFBuffer;
    Ipp32u              *m_pES2PIDTbl;
    SplitterInfo        *m_pInfo;
    bool                 m_bFlagStop;

    struct asf_LockedFrame {
         asf_LockedFrame()
         {
            m_pLockedFrame = new MediaData;
            m_IsLockedFlag = false;
         }
         ~asf_LockedFrame()
         {
             delete m_pLockedFrame;
         }

         MediaData    *m_pLockedFrame;
         bool          m_IsLockedFlag;

       private:
         asf_LockedFrame(const asf_LockedFrame &);
         void operator=(asf_LockedFrame &);
     };

    asf_LockedFrame     **m_ppLockedFrame;
    vm_thread           *m_pReadDataPacketThread;

};

} // namespace UMC

#endif //__UMC_ASF_SPL_H__
