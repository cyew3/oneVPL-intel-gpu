//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_LOG_H
#define __UMC_H264_LOG_H

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_heap.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_frame.h"
#include "umc_h264_frame_info.h"

#include "vm_file.h"

//#define ENABLE_LOGGING
#define H264_LOG_FILE_NAME VM_STRING(".\\h264dec.log")

namespace UMC
{

class Logging
{
#ifdef ENABLE_LOGGING
public:

    Logging();
    ~Logging();

    void LogFrame(H264DecoderFrame * pFrame);

private:

    void Trace(vm_char * format, ...);

    void LogRefFrame(H264DecoderFrame * pFrame);

    H264DecoderFrame * m_pFrame;

    struct Stat
    {
        Ipp32s mbtypes[20];

        Ipp32s subtype_8x8_Count;
        Ipp32s subtype_8x4_Count;
        Ipp32s subtype_4x8_Count;
        Ipp32s subtype_4x4_Count;

        Ipp32s zeroMVCount;

        Stat()
            : zeroMVCount(0)
            , subtype_8x8_Count(0)
            , subtype_8x4_Count(0)
            , subtype_4x8_Count(0)
            , subtype_4x4_Count(0)
        {
            memset(mbtypes, 0, sizeof(mbtypes));
        }

        void Add(Stat & stat)
        {
            for (Ipp32s i = 0; i < sizeof(mbtypes)/sizeof(mbtypes[0]); i++)
            {
                mbtypes[i] += stat.mbtypes[i];
            }

            subtype_8x8_Count += stat.subtype_8x8_Count;
            subtype_8x4_Count += stat.subtype_8x4_Count;
            subtype_4x8_Count += stat.subtype_4x8_Count;
            subtype_4x4_Count += stat.subtype_4x4_Count;

            zeroMVCount += stat.zeroMVCount;
        }
    };

    struct PictureInfo
    {
        Stat stat;
        Ipp32s count;
        Ipp32s bitSize;
        Ipp32s type;
        Ipp32s mbsCount;

        PictureInfo()
            : count(0)
            , bitSize(0)
            , mbsCount(1)
        {
        }
    };

    PictureInfo picturesStat[3];

    void MBLayerStat(H264DecoderGlobalMacroblocksDescriptor &pGlobalInfo, Ipp32s mbNumber, Stat & stat);
    void SBtype(Stat & stat, Ipp32s sbtype);

    Ipp32s CalculateFrameSize(H264DecoderFrame *m_pFrame);
    void PrintStat(Stat &stat, Ipp32s type);

    void PrintPictureStat(PictureInfo & picStat);

    vm_file * m_File;
#endif
};

Logging * GetLogging();

} // namespace UMC

#endif // __UMC_H264_LOG_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
