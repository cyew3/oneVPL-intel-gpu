//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2007 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_statistic.h"

////////////////////////////////////////////////////////////
// Coding time statistics
////////////////////////////////////////////////////////////
#ifdef _PROJECT_STATISTICS_

VC1EncTimeStatistics* m_TStat;

void TimeStatisticsStructureInitialization()
    {
        m_TStat = (VC1EncTimeStatistics*)malloc(sizeof(VC1EncTimeStatistics));
        if(!m_TStat)
        {
            return;
        }

        memset(m_TStat, 0, sizeof(VC1EncTimeStatistics));
    }

void DeleteStatistics()
{
    if(m_TStat)
    {
        free(m_TStat);
        m_TStat = NULL;
    }
}

void WriteStatisticResults()
{
    FILE* statistic_results_file = 0;
    Ipp64f frequency = 1;
    UMC::sSystemInfo* m_ssysteminfo;
    UMC::SysInfo m_csysinfo;

    m_ssysteminfo = (UMC::sSystemInfo *)m_csysinfo.GetSysInfo();

    frequency = m_ssysteminfo->cpu_freq * 1000 *1000;

    statistic_results_file = fopen("StatResults.csv","r");

    if(statistic_results_file==NULL)
    {
        statistic_results_file = fopen("StatResults.csv","w");

        if(statistic_results_file==NULL)
        {
            return;
        }

        //table title
        fprintf(statistic_results_file,"Stream,  Frame count,  GOP, Bitrate, MESpeed,");
        fprintf(statistic_results_file,"Common time,");
        fprintf(statistic_results_file,"ME time,");
        fprintf(statistic_results_file,"AC/DC coding,");
        fprintf(statistic_results_file,"Interpolation,");
        fprintf(statistic_results_file,"Intra Prediction time,");
        fprintf(statistic_results_file,"Inter Prediction time,");
        fprintf(statistic_results_file,"Fwd quant+transform,");
        fprintf(statistic_results_file,"Inv quant+transform,");
        fprintf(statistic_results_file,"Frame reconstruction,");
        fprintf(statistic_results_file,"Deblocking");

        fprintf(statistic_results_file,"\n");
    }
    else
    {
        fclose(statistic_results_file);

        statistic_results_file = fopen("StatResults.csv","a");

        if(statistic_results_file==NULL)
        {
            return;
        }
    }

    //streamName
    if(m_TStat->streamName)
        fprintf(statistic_results_file,"%s,",m_TStat->streamName);
    else
        fprintf(statistic_results_file,"%s,","NoName");

    //Frame count
    fprintf(statistic_results_file,"%d,",m_TStat->frameCount);

    //GOP
    fprintf(statistic_results_file,"(%d-%d),",m_TStat->GOPLen, m_TStat->BLen);

    //Bitrate
    fprintf(statistic_results_file,"%d,",m_TStat->bitrate/1000);

    //MESpeed
    fprintf(statistic_results_file,"%d,",m_TStat->meSpeedSearch);

    //Common time
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->totalTime))/frequency);

    //Motion estimation time
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->me_TotalTime))/frequency);

    //AC/DC coef coding
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->AC_Coefs_TotalTime))/frequency);

    //Interpolation
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->Interpolate_TotalTime))/frequency);

    //Intra prediction
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->Intra_TotalTime))/frequency);

    //Inter prediction
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->Inter_TotalTime))/frequency);

    //Forward quantization and transforming
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->FwdQT_TotalTime))/frequency);

    //Inverse quantization and transforming
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->InvQT_TotalTime))/frequency);

    //Reconstruction
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_TStat->Reconst_TotalTime))/frequency);

    //Deblocking
    fprintf(statistic_results_file,"%lf",((Ipp64f)(m_TStat->Deblk_TotalTime))/frequency);

    fprintf(statistic_results_file,"\n");
    fclose(statistic_results_file);
}
#endif //_PROJECT_STATISTICS_

////////////////////////////////////////////////////////////
// IPP functions statistics
////////////////////////////////////////////////////////////

#ifdef _VC1_IPP_STATISTICS_
VC1EncIppStatistics* m_IppStat;

void IppStatisticsStructureInitialization()
{
    m_IppStat = (VC1EncIppStatistics*)malloc(sizeof(VC1EncIppStatistics));
    if(!m_IppStat)
    {
        return;
    }
    memset(m_IppStat, 0, sizeof(VC1EncIppStatistics));
}

void DeleteIppStatistics()
{
    if(m_IppStat)
    {
        free(m_IppStat);
        m_IppStat = NULL;
    }
}

void WriteIppStatisticResults()
{
    FILE* statistic_results_file = 0;
    Ipp64f frequency = 1;
    UMC::sSystemInfo* m_ssysteminfo;
    UMC::SysInfo m_csysinfo;

    m_ssysteminfo = (UMC::sSystemInfo *)m_csysinfo.GetSysInfo();

    frequency = m_ssysteminfo->cpu_freq * 1000 *1000;

    statistic_results_file = fopen("IppStatResults.csv","r");

    if(statistic_results_file==NULL)
    {
        statistic_results_file = fopen("IppStatResults.csv","w");

        if(statistic_results_file==NULL)
        {
            return;
        }

        //table title
        fprintf(statistic_results_file,"Stream,  Frame count,  GOP, Bitrate, MESpeed,");
        fprintf(statistic_results_file,"Common time,Ipp time");
        fprintf(statistic_results_file,"\n");
    }
    else
    {
        fclose(statistic_results_file);

        statistic_results_file = fopen("IppStatResults.csv","a");

        if(statistic_results_file==NULL)
        {
            return;
        }
    }

    //streamName
    if(m_IppStat->streamName)
        fprintf(statistic_results_file,"%s,",m_IppStat->streamName);
    else
        fprintf(statistic_results_file,"%s,","NoName");

    //Frame count
    fprintf(statistic_results_file,"%d,",m_IppStat->frameCount);

    //GOP
    fprintf(statistic_results_file,"(%d-%d),",m_IppStat->GOPLen, m_IppStat->BLen);

    //Bitrate
    fprintf(statistic_results_file,"%d,",m_IppStat->bitrate/1000);

    //MESpeed
    fprintf(statistic_results_file,"%d,",m_IppStat->meSpeedSearch);

    //Common time
    fprintf(statistic_results_file,"%lf,",((Ipp64f)(m_IppStat->totalTime))/frequency);

    //Ipp time
    fprintf(statistic_results_file,"%lf",((Ipp64f)(m_IppStat->IppTotalTime))/frequency);

    fprintf(statistic_results_file,"\n");
    fclose(statistic_results_file);
}
#endif
#endif //UMC_ENABLE_VC1_VIDEO_ENCODER
