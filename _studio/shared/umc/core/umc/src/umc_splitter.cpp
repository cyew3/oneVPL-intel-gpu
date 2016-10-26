//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include <string.h>
#include "umc_splitter.h"
#include "mfxstructures.h"

#if defined (__ICL)
// multicharacter character literal (potential portability problem)
// we support Win platform only
#pragma warning(disable:1899)
#endif

#define MAKEFOURCC_INVERT(D,C,B,A)    ((((int)A))+(((int)B)<<8)+(((int)C)<<16)+(((int)D)<<24))

namespace UMC
{

SplitterParams::SplitterParams()
{
   m_lFlags = 0;
   m_pDataReader = NULL;
   m_uiSelectedVideoPID = SELECT_ANY_VIDEO_PID;
   m_uiSelectedAudioPID = SELECT_ANY_AUDIO_PID;
   m_pMemoryAllocator = NULL;
} // SplitterParams::SplitterParams()

SplitterParams::~SplitterParams()
{
} // SplitterParams::~SplitterParams()

SplitterInfo::SplitterInfo()
{
    m_splitter_flags = 0;
    m_SystemType = UNDEF_STREAM;
    m_nOfTracks = 0;
    m_dRate = 1;
    m_dDuration = -1.0;
    m_ppTrackInfo = NULL;
    DeprecatedSplitterInfo();
} // SplitterInfo::SplitterInfo()

SplitterInfo::~SplitterInfo()
{
} // SplitterInfo::~SplitterInfo()

Splitter::Splitter():
  m_pDataReader(NULL)
{
    DeprecatedSplitter(); // Init deprecated variables
}

SystemStreamType Splitter::GetStreamType(DataReader* dr)
{
    Ipp32u long_code;
    Status umcSts = UMC_OK;

    if (NULL == dr)
        return UNDEF_STREAM;
    else
        dr->Reset();

    umcSts = dr->Check32u(&long_code, 0);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    // it can be either avs or mpeg4 format
    if (long_code == 0x000001B0)
    {
        Ipp8u oneByte;

        // the header of avs standard is 18 bytes long.
        // the one of mpeg4 standard is only one byte long.
        umcSts = dr->Check8u(&oneByte, 5);
        if (UMC_OK != umcSts)
            return UNDEF_STREAM;

        if (oneByte)
            return AVS_PURE_VIDEO_STREAM;
    }

    if (0x80000001 == long_code)
    {
        Ipp8u oneByte;

        // it is known bug of avs reference encoder -
        // it adds extra 0x080 byte at the beginning.
        umcSts = dr->Check8u(&oneByte, 4);
        if (UMC_OK != umcSts)
            return UNDEF_STREAM;
        if (oneByte == 0x0B0)
        {
            return AVS_PURE_VIDEO_STREAM;
        }
    }

    if (long_code == 0x0000010F || (long_code&0xFF) == 0xC5)
    {
        return VC1_PURE_VIDEO_STREAM;
    }

    if (long_code == 0x3026b275)
    {
        return ASF_STREAM;
    }

    if (long_code == MAKEFOURCC_INVERT('R','I','F','F')) // RIFF
    {
        umcSts = dr->Check32u(&long_code, 8);
        if (long_code == MAKEFOURCC_INVERT('A','V','I',' '))
        {
            //avi RIFF container
            return AVI_STREAM;
        }
    }
    if (long_code == 0x464c5601) // "FLV 0x01"  FLV version1
    {
        return FLV_STREAM;
    }
    //IVF : http://wiki.multimedia.cx/index.php?title=IVF
    if (long_code == MAKEFOURCC_INVERT('D','K','I','F'))
    {
        return IVF_STREAM;
    }

    umcSts = dr->Check32u(&long_code, 4);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    if (long_code == MAKEFOURCC_INVERT('f','t','y','p'))
    {
        umcSts = dr->Check32u(&long_code,8);
        if (UMC_OK != umcSts) return UNDEF_STREAM;

        // mp42
        if (long_code == MAKEFOURCC_INVERT('m','p','4','2') ||
            long_code == MAKEFOURCC_INVERT('m','p','4','1') ||
            long_code == MAKEFOURCC_INVERT('i','s','o','m') ||
            long_code == MAKEFOURCC_INVERT('i','s','o','4') ||
            long_code == MAKEFOURCC_INVERT('M','S','N','V') ||
            long_code == MAKEFOURCC_INVERT('M','4','V',' ') ||
            long_code == MAKEFOURCC_INVERT('M','4','A',' ') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','6') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','4') ||
            long_code == MAKEFOURCC_INVERT('3','g','p','5') ||
            long_code == MAKEFOURCC_INVERT('a','v','c','1') ||
            long_code == MAKEFOURCC_INVERT('a','v','s','2') ||
            long_code == MAKEFOURCC_INVERT('q','t',' ',' ') ||
            long_code == MAKEFOURCC_INVERT('X','A','V','C'))
        {
            //MP4 container
            return MP4_ATOM_STREAM;
        }
    }

    umcSts = dr->Check32u(&long_code,4);
    if (UMC_OK != umcSts)
        return UNDEF_STREAM;

    if (long_code == MAKEFOURCC_INVERT('m','o','o','v'))
    {
        return MP4_ATOM_STREAM;
    }

    return MPEGx_SYSTEM_STREAM;
} // SystemStreamType Splitter::GetStreamType(DataReader* dr)

} // namespace UMC
