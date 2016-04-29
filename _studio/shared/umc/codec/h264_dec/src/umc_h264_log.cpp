/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_log.h"

#ifdef ENABLE_LOGGING
#include <stdarg.h>
#endif

namespace UMC
{
#ifdef ENABLE_LOGGING
    Logging * GetLogging()
    {
        static Logging log;
        return &log;
    }
#else
    Logging * GetLogging()
    {
        return 0;
    }
#endif

static const vm_char * frameTypes[] =
{
    VM_STRING("None"),
    VM_STRING("I_PICTURE"),
    VM_STRING("P_PICTURE"),
    VM_STRING("B_PICTURE")
};

#ifdef ENABLE_LOGGING

Logging::Logging()
{
    m_File = vm_file_fopen(H264_LOG_FILE_NAME, VM_STRING("rw+"));

    picturesStat[0].type = 1;
    picturesStat[1].type = 2;
    picturesStat[2].type = 3;
}

Logging::~Logging()
{
    Trace(VM_STRING("\n"));
    PrintPictureStat(picturesStat[0]);
    PrintPictureStat(picturesStat[1]);
    PrintPictureStat(picturesStat[2]);
    vm_file_close(m_File);
}

void Logging::PrintPictureStat(PictureInfo & picStat)
{
    if (picStat.count)
    {
        Trace(VM_STRING("\n"));
        Trace(VM_STRING("%s frames - %d, size in bits - %d, avg size - %d\n"),
            frameTypes[picStat.type],
            picStat.count,
            picStat.bitSize, picStat.bitSize / picStat.count);
        PrintStat(picStat.stat, picStat.type);
    }
}

void Logging::Trace(vm_char * format, ...)
{
    va_list arglist;
    va_start(arglist, format);

    vm_char cStr[256];
    vm_string_vsprintf(cStr, format, arglist);

    vm_string_fprintf(m_File, VM_STRING("%s"), cStr);
    va_end(arglist);
}

void Logging::SBtype(Stat & stat, Ipp32s sbtype)
{
    switch(sbtype)
    {
    case SBTYPE_8x8:
        stat.subtype_8x8_Count++;
        break;
    case SBTYPE_8x4:
        stat.subtype_8x4_Count++;
        break;
    case SBTYPE_4x8:
        stat.subtype_4x8_Count++;
        break;

    case SBTYPE_DIRECT:
    case SBTYPE_4x4:
    default:
        stat.subtype_4x4_Count++;
        break;
    }
}

void Logging::MBLayerStat(H264DecoderGlobalMacroblocksDescriptor &pGlobalInfo, Ipp32s mbNumber, Stat & stat)
{
    H264DecoderMacroblockGlobalInfo &pMBInfo = pGlobalInfo.mbs[mbNumber];

    if (pMBInfo.mbflags.isSkipped)
    {
        stat.mbtypes[MBTYPE_SKIPPED]++;
    }else
    {
        if (pMBInfo.mbflags.isDirect)
        {
            stat.mbtypes[MBTYPE_DIRECT]++;
        }
        else
        {
            if (pMBInfo.mbtype == MBTYPE_FORWARD ||
                pMBInfo.mbtype == MBTYPE_BACKWARD ||
                pMBInfo.mbtype == MBTYPE_BIDIR)
            {
                stat.mbtypes[MBTYPE_INTER]++;
            }
            else
            {
                if (pMBInfo.mbtype == MBTYPE_INTER_8x8 ||
                    pMBInfo.mbtype == MBTYPE_INTER_8x8_REF0)
                {
                    stat.mbtypes[pMBInfo.mbtype]++;
                    SBtype(stat, pMBInfo.sbtype[0]);
                    SBtype(stat, pMBInfo.sbtype[1]);
                    SBtype(stat, pMBInfo.sbtype[2]);
                    SBtype(stat, pMBInfo.sbtype[3]);
                }
                else
                {
                    stat.mbtypes[pMBInfo.mbtype]++;
                }
            }
        }
    }

    // mbtype stat
    stat.zeroMVCount++;

    for (Ipp32s i = 0; i < 16; i++)
    {
        if (pGlobalInfo.MV[0][mbNumber].MotionVectors[i].mvx || pGlobalInfo.MV[0][mbNumber].MotionVectors[i].mvy)
        {
            stat.zeroMVCount--;
            break;
        }

        if (pGlobalInfo.MV[1][mbNumber].MotionVectors[i].mvx || pGlobalInfo.MV[1][mbNumber].MotionVectors[i].mvy)
        {
            stat.zeroMVCount--;
            break;
        }
    }
}

Ipp32s Logging::CalculateFrameSize(H264DecoderFrame *m_pFrame)
{
    H264DecoderFrameInfo * slicesInfo = m_pFrame->GetAU(0);

    Ipp32s size = 0;

    for (Ipp32u i = 0; i < slicesInfo->GetSliceCount(); i++)
    {
        H264Slice * pSlice = slicesInfo->GetSlice(i);
        size += (Ipp32s)pSlice->GetBitStream()->GetAllBitsCount();
    }

    return size;
}

void Logging::LogFrame(H264DecoderFrame * pFrame)
{
    m_pFrame = pFrame;

    Trace(VM_STRING("\n"));
    Ipp32s bitSize = CalculateFrameSize(m_pFrame);
    Trace(VM_STRING("frame type - %s, uid - %d, size - %d\n"), frameTypes[m_pFrame->m_FrameType], m_pFrame->m_UID, bitSize);

    LogRefFrame(m_pFrame);

#ifdef UMC_RESTRICTED_CODE_VA
    H264DecoderFrameInfo * slicesInfo = m_pFrame->GetAU(0);
    const H264SeqParamSet *pSeqParam = slicesInfo->GetSlice(0)->GetSeqParam();

    Ipp32s iMBCount = pSeqParam->frame_width_in_mbs * pSeqParam->frame_height_in_mbs;

    Stat stat;
    for (Ipp32s x = 0; x < iMBCount; ++x)
    {
        MBLayerStat(m_pFrame->m_mbinfo, x, stat);
    }

    PictureInfo & currentPicture = picturesStat[m_pFrame->m_FrameType - 1];
    PrintStat(stat, currentPicture.type);

    currentPicture.count++;
    currentPicture.stat.Add(stat);
    currentPicture.bitSize += bitSize;
    currentPicture.mbsCount = iMBCount;
#endif
}

void Logging::LogRefFrame(H264DecoderFrame * pFrame)
{
    H264DecoderRefPicList* refPicList = pFrame->GetRefPicList(1, 0);

    if (refPicList->m_RefPicList[0])
    {
        Trace(VM_STRING("\n ref list 0 \n"));
        for (Ipp32s i = 0; refPicList->m_RefPicList[i]; i++)
        {
            Trace(VM_STRING("ref frame uid - %d\n"), refPicList->m_RefPicList[i]->m_UID);
        }
    }

    refPicList = pFrame->GetRefPicList(1, 1);

    if (refPicList->m_RefPicList[0])
    {
        Trace(VM_STRING("\n ref list 1 \n"));
        for (Ipp32s i = 0; refPicList->m_RefPicList[i]; i++)
        {
            Trace(VM_STRING("ref frame uid - %d\n"), refPicList->m_RefPicList[i]->m_UID);
        }
        Trace(VM_STRING("\n"));
    }
}

void Logging::PrintStat(Stat &stat, Ipp32s type)
{
    Trace(VM_STRING("I16x16 - %d\n"), stat.mbtypes[MBTYPE_INTRA_16x16]);
    Trace(VM_STRING("I4x4 - %d\n"), stat.mbtypes[MBTYPE_INTRA]);

    if (type == I_PICTURE)
        return;

    Trace(VM_STRING("Inter16x16 - %d\n"), stat.mbtypes[MBTYPE_INTER]);
    Trace(VM_STRING("Inter16x8 - %d\n"), stat.mbtypes[MBTYPE_INTER_16x8]);
    Trace(VM_STRING("Inter8x16 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x16]);
    Trace(VM_STRING("Inter8x8 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x8]);
    Trace(VM_STRING("Inter8x8_Ref0 - %d\n"), stat.mbtypes[MBTYPE_INTER_8x8_REF0]);
    Trace(VM_STRING(" subtype 8x8 - %d\n"), stat.subtype_8x8_Count);
    Trace(VM_STRING(" subtype 8x4 - %d\n"), stat.subtype_8x4_Count);
    Trace(VM_STRING(" subtype 4x8 - %d\n"), stat.subtype_4x8_Count);
    Trace(VM_STRING(" subtype 4x4 - %d\n"), stat.subtype_4x4_Count);

    if (type == B_PICTURE)
        Trace(VM_STRING("Direct - %d\n"), stat.mbtypes[MBTYPE_DIRECT]);

    Trace(VM_STRING("Skipped - %d\n"), stat.mbtypes[MBTYPE_SKIPPED]);
    Trace(VM_STRING("Zero MVs count - %d\n"), stat.zeroMVCount);
}

#endif

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
