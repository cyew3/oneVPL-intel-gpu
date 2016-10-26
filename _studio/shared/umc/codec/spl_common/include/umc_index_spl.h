//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2008 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_INDEX_SPLITTER_H__
#define __UMC_INDEX_SPLITTER_H__

#include "umc_splitter.h"
#include "umc_splitter_ex.h"
#include "umc_mutex.h"

#include "umc_thread.h"
#include "umc_index.h"
#include "umc_media_buffer.h"

namespace UMC
{

class IndexSplitter : public Splitter
{

    DYNAMIC_CAST_DECL(IndexSplitter, Splitter)

public:
    IndexSplitter();
    ~IndexSplitter();

    virtual Status Init(SplitterParams &init_params);

    virtual Status Close();

    virtual Status Stop() {return StopSplitter();}
    virtual Status Run();

    virtual Status GetNextData(MediaData* data, Ipp32u nTrack);
    virtual Status CheckNextData(MediaData* data, Ipp32u nTrack);

    virtual Status SetTimePosition(Ipp64f position); // in second
    virtual Status GetTimePosition(Ipp64f& position); // return current position in second
    virtual Status SetRate(Ipp64f rate);

    // Get splitter info
    virtual Status GetInfo(SplitterInfo** ppInfo);

    // changes state of track
    // iState = 0 means disable, iState = 1 means enable
    virtual Status EnableTrack(Ipp32u nTrack, Ipp32s iState);

protected:
    virtual Status StopSplitter();

    static Ipp32u  VM_THREAD_CALLCONVENTION  ReadESThreadCallback(void* ptr);
    void    ReadES(Ipp32u uiPin);

    bool                 m_bFlagStop;
    vm_thread           *m_pReadESThread;

    DataReader          *m_pReader;
    Mutex                m_ReaderMutex;
    TrackIndex          *m_pTrackIndex;
    MediaBuffer        **m_ppMediaBuffer;
    MediaData          **m_ppLockedFrame;
    Ipp32s              *m_pIsLocked;   /* since (m_ppLockedFrame[i]->GetDataSize == 0) is valid */
    SplitterInfo        *m_pInfo;

    DataReader *m_pDataReader;
    Mutex m_dataReaderMutex;
};

} // namespace UMC

#endif //__UMC_INDEX_SPLITTER_H__
