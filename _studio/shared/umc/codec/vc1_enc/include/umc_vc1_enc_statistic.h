// Copyright (c) 2004-2018 Intel Corporation
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
            unsigned long long frameCount;
            vm_char streamName[256];
            int32_t bitrate;
            int32_t GOPLen;
            int32_t BLen;

            unsigned long long startTime;                 //application start time
            unsigned long long endTime;                   // aplication end time
            unsigned long long totalTime;                 // total time

            //Motion estimation
            unsigned long long me_StartTime;
            unsigned long long me_EndTime;
            unsigned long long me_TotalTime;

            int32_t meSpeedSearch;

            //Interpolation
            unsigned long long Interpolate_StartTime;
            unsigned long long Interpolate_EndTime;
            unsigned long long Interpolate_TotalTime;

            //Intra prediction
            unsigned long long Intra_StartTime;
            unsigned long long Intra_EndTime;
            unsigned long long Intra_TotalTime;

            //Inter prediction
            unsigned long long Inter_StartTime;
            unsigned long long Inter_EndTime;
            unsigned long long Inter_TotalTime;

            //Forward quantization and transforming
            unsigned long long FwdQT_StartTime;
            unsigned long long FwdQT_EndTime;
            unsigned long long FwdQT_TotalTime;

            //Inverse quantization and transforming
            unsigned long long InvQT_StartTime;
            unsigned long long InvQT_EndTime;
            unsigned long long InvQT_TotalTime;

            //Inverse quantization and transforming
            unsigned long long Reconst_StartTime;
            unsigned long long Reconst_EndTime;
            unsigned long long Reconst_TotalTime;

            //Deblocking
            unsigned long long Deblk_StartTime;
            unsigned long long Deblk_EndTime;
            unsigned long long Deblk_TotalTime;

            //Coding AC coeffs
            unsigned long long AC_Coefs_StartTime;
            unsigned long long AC_Coefs_EndTime;
            unsigned long long AC_Coefs_TotalTime;

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
            unsigned long long frameCount;
            vm_char streamName[256];
            int32_t bitrate;
            int32_t GOPLen;
            int32_t BLen;

            unsigned long long startTime;                 //application start time
            unsigned long long endTime;                   // aplication end time
            unsigned long long totalTime;                 // total time

            unsigned long long IppStartTime;                 //application start time
            unsigned long long IppEndTime;                   // aplication end time
            unsigned long long IppTotalTime;                 // total time

            int32_t meSpeedSearch;            //ME speed
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
