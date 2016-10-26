//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _UMC_VC1_ENC_STATISTIC_H_
#define _UMC_VC1_ENC_STATISTIC_H_

#include "ippvc.h"
#include "umc_structures.h"

//#define VC1_ME_MB_STATICTICS

//# define _PROJECT_STATISTICS_

#ifdef _PROJECT_STATISTICS_
    #include "vm_time.h"
    #include "umc_sys_info.h"
    #include "vm_sys_info.h"
    #include <string.h>

        typedef struct
        {
            Ipp64u frameCount;
            vm_char streamName[256];
            Ipp32s bitrate;
            Ipp32s GOPLen;
            Ipp32s BLen;

            Ipp64u startTime;                 //application start time
            Ipp64u endTime;                   // aplication end time
            Ipp64u totalTime;                 // total time

            //Motion estimation
            Ipp64u me_StartTime;
            Ipp64u me_EndTime;
            Ipp64u me_TotalTime;

            Ipp32s meSpeedSearch;

            //Interpolation
            Ipp64u Interpolate_StartTime;
            Ipp64u Interpolate_EndTime;
            Ipp64u Interpolate_TotalTime;

            //Intra prediction
            Ipp64u Intra_StartTime;
            Ipp64u Intra_EndTime;
            Ipp64u Intra_TotalTime;

            //Inter prediction
            Ipp64u Inter_StartTime;
            Ipp64u Inter_EndTime;
            Ipp64u Inter_TotalTime;

            //Forward quantization and transforming
            Ipp64u FwdQT_StartTime;
            Ipp64u FwdQT_EndTime;
            Ipp64u FwdQT_TotalTime;

            //Inverse quantization and transforming
            Ipp64u InvQT_StartTime;
            Ipp64u InvQT_EndTime;
            Ipp64u InvQT_TotalTime;

            //Inverse quantization and transforming
            Ipp64u Reconst_StartTime;
            Ipp64u Reconst_EndTime;
            Ipp64u Reconst_TotalTime;

            //Deblocking
            Ipp64u Deblk_StartTime;
            Ipp64u Deblk_EndTime;
            Ipp64u Deblk_TotalTime;

            //Coding AC coeffs
            Ipp64u AC_Coefs_StartTime;
            Ipp64u AC_Coefs_EndTime;
            Ipp64u AC_Coefs_TotalTime;

        }VC1EncTimeStatistics;

        extern VC1EncTimeStatistics* m_TStat;

        #define STATISTICS_START_TIME(startTime) (startTime) = ippGetCpuClocks();
        #define STATISTICS_END_TIME(startTime, endTime, totalTime) \
                (endTime)=ippGetCpuClocks();                       \
                (totalTime) += ((endTime) - (startTime));

        void TimeStatisticsStructureInitialization();
        void WriteStatisticResults();
        void DeleteStatistics();

#else

    #include "vm_time.h"

        #define STATISTICS_START_TIME(startTime)
        #define STATISTICS_END_TIME(startTime,endTime,totalTime)

#endif

//#define _VC1_IPP_STATISTICS_
#ifdef _VC1_IPP_STATISTICS_
    #include "vm_time.h"
    #include "umc_sys_info.h"
    #include "vm_sys_info.h"
    #include <string.h>

        typedef struct
        {
            Ipp64u frameCount;
            vm_char streamName[256];
            Ipp32s bitrate;
            Ipp32s GOPLen;
            Ipp32s BLen;

            Ipp64u startTime;                 //application start time
            Ipp64u endTime;                   // aplication end time
            Ipp64u totalTime;                 // total time

            Ipp64u IppStartTime;                 //application start time
            Ipp64u IppEndTime;                   // aplication end time
            Ipp64u IppTotalTime;                 // total time

            Ipp32s meSpeedSearch;            //ME speed
        }VC1EncIppStatistics;

        extern VC1EncIppStatistics* m_IppStat;

        #define IPP_STAT_START_TIME(startTime) (startTime) = ippGetCpuClocks();
        #define IPP_STAT_END_TIME(startTime, endTime, totalTime) \
                (endTime)=ippGetCpuClocks();                       \
                (totalTime) += ((endTime) - (startTime));

        void IppStatisticsStructureInitialization();
        void WriteIppStatisticResults();
        void DeleteIppStatistics();

#else

    #include "vm_time.h"

        #define IPP_STAT_START_TIME(startTime)
        #define IPP_STAT_END_TIME(startTime,endTime,totalTime)

#endif//_VC1_IPP_STATISTICS_

#endif //_UMC_VC1_ENC_STATISTIC_H
#endif //UMC_ENABLE_VC1_VIDEO_ENCODER
