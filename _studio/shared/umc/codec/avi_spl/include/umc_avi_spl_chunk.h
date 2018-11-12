// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_AVI_SPL_CHUNK_H__
#define __UMC_AVI_SPL_CHUNK_H__

#include "umc_mutex.h"
#include "umc_data_reader.h"
#include "umc_avi_types.h"

#define MAX_AVI_CHUNK_DEPTH (6)

namespace UMC
{

    class AVIChunk
    {
    public:
        AVIChunk();
        Status Init(DataReader *pDataReader, Mutex *pMutex);
        Status DescendChunk(tFOURCC chnkName);
        Status DescendLIST(tFOURCC chnkName);
        Status DescendRIFF(tFOURCC chnkName);
        void GoChunkHead();
        Status Ascend();
        Ipp32u GetChunkHead();
        tFOURCC GetChunkFOURCC();
        Ipp32u GetChunkSize();
        Status GetData(Ipp8u* pbBuffer, Ipp32u uiBufSize);
        Status GetData(Ipp64u uiOffset, Ipp8u* pbBuffer, Ipp32u uiBufSize);
        static bool CmpChunkNames(const tFOURCC chnkName1, const tFOURCC chnkName2);
        Status JumpToFilePos(const Ipp64u uiFilePos);

        virtual ~AVIChunk(){}
    protected:
        Status DescendChunkList(tFOURCC chnkName, tFOURCC listName);

        class CChnkInfo
        {
        public:
            tFOURCC m_chnkName;
            Ipp64u m_stSize;
            Ipp64u m_stFilePos;
            CChnkInfo():m_chnkName(0),m_stSize(0),m_stFilePos(0){}
        };

        CChnkInfo m_ChnkStack[MAX_AVI_CHUNK_DEPTH];
        Ipp32u m_ulStackPos;
        DataReader *m_pReader;
        Mutex *m_pReaderMutex;
        Ipp64u m_uiPos;
    };

}  //  namespace UMC

#endif  //   __UMC_AVI_SPL_CHUNK_H__
