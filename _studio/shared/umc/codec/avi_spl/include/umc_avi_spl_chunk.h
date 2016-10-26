//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2008 Intel Corporation. All Rights Reserved.
//

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
