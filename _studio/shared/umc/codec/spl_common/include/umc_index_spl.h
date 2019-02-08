// Copyright (c) 2007-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __UMC_INDEX_SPLITTER_H__
#define __UMC_INDEX_SPLITTER_H__

#include "umc_splitter.h"
#include "umc_splitter_ex.h"
#include "umc_mutex.h"

#include "vm_thread.h"
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
