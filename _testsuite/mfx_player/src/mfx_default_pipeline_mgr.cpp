/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "vm_sys_info.h"
#include "mfx_pipeline_defs.h"
#include "test_statistics.h"
#include "mfx_default_pipeline_mgr.h"
#include "mfx_decode_pipeline_config.h"
#include "ippi.h"
#include <iostream>

#define LRB_FLASH_TOOL  "C:\\Program Files (x86)\\Intel\\LrbFlashUI\\LRBFlashCLI.exe"
#define LRB_FLASH_ARG   "-getVersion all -imageSrc HW"
#define LRB_FLASH_LOG   "c:\\lrb_flash_version.txt"
#define DATETIME_FORMAT VM_STRING("%d/%02d/%02d %d:%02d:%02d")

#define GO_OR_PRINT_HELP(action)\
    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pPipeline->action, pPipeline->PrintHelp());

#define GO_OR_QUIT(action)\
    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pPipeline->action, vm_string_printf(VM_STRING("\nERROR: %s \nFAILED\n"), pPipeline->GetLastErrString()));

#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

int MFXPipelineManager::Execute(IMFXPipelineConfig *pCfg)throw()
{
    try
    {
        std::auto_ptr<IMFXPipeline> pPipeline;
        MFX_CHECK_WITH_ERR(NULL != (pPipeline.reset(pCfg->CreatePipeline()), pPipeline.get()), 1);

        mfxU32 nTimeout   = 0;
        mfxU32 nRepeat    = 0;
        bool   bGPUHangRecovery = false;
        Timer  reliabilityTimer;
        vm_char  sTmp[256] = VM_STRING("");

        ippStaticInit();

        if (NULL == pPipeline.get())
        {
            PipelineTrace((VM_STRING("Out of Memory. Can't Create Pipeline\n")));
            return 1;
        }

        //attaching external synhronizer
        pPipeline->SetSyncro(pCfg->GetExternalSync());

        std::vector<tstring> ReconstrcutedArgs;
        MFX_CHECK_STS(pPipeline->ReconstructInput(pCfg->GetArgv(), pCfg->GetArgc(), &ReconstrcutedArgs));

        PipelineTrace((VM_STRING("\n-----------------------------------------------------------------------------------------\n")));

        PipelineTrace((VM_STRING("Command-line: ")));
        for (std::vector<tstring>::size_type i = 0; i < ReconstrcutedArgs.size(); i++)
        {
            PipelineTrace((VM_STRING("%s "), ReconstrcutedArgs[i].c_str()));
        }
        PipelineTrace((VM_STRING("\n")));

        vm_sys_info_get_computer_name(sTmp);
        PrintInfo(VM_STRING("ComputerName"), VM_STRING("%s"), sTmp);
        vm_sys_info_get_vga_card(sTmp);
        PrintInfo(VM_STRING("GraphicName"), VM_STRING("%s"), sTmp);

        //print IPP info
        const IppLibraryVersion* ippVersion = ippiGetLibVersion();
        PrintInfo(VM_STRING("IPP"          ), VM_STRING("%hs"), ippVersion->Version);
        PrintInfo(VM_STRING("  Build"      ), VM_STRING("%i" ), ippVersion->build);
        PrintInfo(VM_STRING("  Target CPU" ), VM_STRING("%hs"), ippVersion->targetCpu);
        PrintInfo(VM_STRING("  Name"       ), VM_STRING("%hs"), ippVersion->Name);
        PrintInfo(VM_STRING("  Build date" ), VM_STRING("%hs"), ippVersion->BuildDate);

    #if defined(_WIN32) || defined(_WIN64)
        // Print current and boot time
        __INT64 boot_time = 0;
        SYSTEMTIME stime, btime;
        GetLocalTime(&stime);
        SystemTimeToFileTime(&stime, (FILETIME*)&boot_time);
        if (boot_time) boot_time -= 10000*GetTickCount();
        FileTimeToSystemTime((FILETIME*)&boot_time, &btime);

        int up_time = (int)(GetTickCount() / 1000);

        PrintInfo(VM_STRING("Time"), DATETIME_FORMAT, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond);
        PrintInfo(VM_STRING("Boot time"), DATETIME_FORMAT, btime.wYear, btime.wMonth, btime.wDay, btime.wHour, btime.wMinute, btime.wSecond);
        PrintInfo(VM_STRING("Up time"), VM_STRING("%d:%02d:%02d"), up_time/(60*60), (up_time/60) % 60, up_time % 60);

        // Print LRB Flash version
        if (GetFileAttributesA(LRB_FLASH_TOOL) != 0xFFFFFFFF)
        {
            HANDLE hFile;
            __INT64 logfile_time = 0;

            hFile = CreateFileA(LRB_FLASH_LOG, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            GetFileTime(hFile, 0, 0, (FILETIME*)&logfile_time);
            FileTimeToLocalFileTime((FILETIME*)&logfile_time, (FILETIME*)&logfile_time);
            FileTimeToSystemTime((FILETIME*)&logfile_time, &stime);
            CloseHandle(hFile);
            PrintInfo(VM_STRING("LRB version file"), DATETIME_FORMAT, stime.wYear, stime.wMonth, stime.wDay, stime.wHour, stime.wMinute, stime.wSecond);
            //PrintDllInfo(VM_STRING("LRB version file"), VM_STRING( LRB_FLASH_LOG ));

            // call Flash Utility once after reboot
            if (logfile_time < boot_time || !logfile_time || !boot_time)
            {
                system("\"" LRB_FLASH_TOOL "\" " LRB_FLASH_ARG " > " LRB_FLASH_LOG);
            }

            FILE *f = fopen(LRB_FLASH_LOG, "rt");
            if (f)
            {
                vm_char str[1024], *p;
                while (fgetws(str, MFX_ARRAY_SIZE(str), f))
                {
                    if (vm_string_strstr(str, VM_STRING("Version")) && NULL != (p = vm_string_strchr(str, ':')))
                    {
                        *p++ = 0;
                        PrintInfo(str, p);
                    }
                }
                fclose(f);
            }
        }
    #endif // #if defined(_WIN32) || defined(_WIN64)

        vm_char ** argv =  pCfg->GetArgv();
        int argc = pCfg->GetArgc();

        GO_OR_PRINT_HELP(ProcessCommand(++argv, --argc));

        GO_OR_QUIT(GetMulTipleAndReliabilitySettings(nRepeat, nTimeout));

        GO_OR_QUIT(GetGPUErrorHandlingSettings(bGPUHangRecovery));

        // nRepeat = 3;
        for (mfxU32 i = 0; i < nRepeat; i++)
        {
            if (nRepeat > 1 && 0 == nTimeout)
            {
                PipelineTrace((VM_STRING("----------------%d / %d pass--------------\n"), i + 1, nRepeat));
            }
            if (nTimeout != 0)
            {
                PipelineTrace((VM_STRING("--------------%d / %.2f / %.2f sec--------------\n")
                    , i + 1
                    , reliabilityTimer.OverallTiming()
                    , (double)nTimeout));
                reliabilityTimer.Start();
            }

            if (1 == i)
            {
                vm_char pDst [MAX_FILE_PATH];
                GO_OR_QUIT(GetOutFile(pDst, MFX_ARRAY_SIZE(pDst)));
                GO_OR_QUIT(SetOutFile(NULL));
                GO_OR_QUIT(SetRefFile(pDst, 0));
            }

            mfxStatus sts        = MFX_ERR_MEMORY_ALLOC;
            bool      bReduceMem = false;

            for (;MFX_ERR_MEMORY_ALLOC  == sts;)
            {
                MFX_CLEAR_LASTERR();

                while(bReduceMem || MFX_ERR_MEMORY_ALLOC == (sts = pPipeline->BuildPipeline()))
                {
                    MFX_CLEAR_LASTERR();
                    //reducing required memory
                    vm_string_printf(VM_STRING("\nERROR: Memory Allocating Error, trying to reduce memory usage\n"));
                    GO_OR_QUIT(ReduceMemoryUsage());
                    bReduceMem = false;
                }

                MFX_CHECK_STS(sts);

                for (;;)
                {
                    MFX_CLEAR_LASTERR();
                    try
                    {
                        sts = pPipeline->Play();
                    }
                    catch (std::bad_alloc & ex)
                    {
                        sts = MFX_ERR_MEMORY_ALLOC;
                        vm_string_printf(VM_STRING("Exception: ")STR_TOKEN(), ex.what());
                    }

                    if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
                    {
                        //reseting all components excluding splitter
                        vm_string_printf(VM_STRING("\nERROR: incompatible params, reseting pipeline ..."));
                        GO_OR_QUIT(LightReset());
                        vm_string_printf(VM_STRING("OK\n"));
                        continue;
                    }

                    if (MFX_ERR_GPU_HANG == sts)
                    {
                        if (bGPUHangRecovery)
                        {
                            vm_string_printf(VM_STRING("\nERROR: GPU hang occured, recreating pipeline\n"));
                            GO_OR_QUIT(HeavyReset());
                            vm_string_printf(VM_STRING("OK\n"));
                            continue;
                        }
                        else
                            vm_string_printf(VM_STRING("\nERROR: GPU hang occured\n"));
                    }

                    if (MFX_ERR_MEMORY_ALLOC == sts)
                    {
                        bReduceMem = true;
                        break;
                    }
                    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == sts, vm_string_printf(VM_STRING("\nFAILED\n")));
                    break;
                }
            }

            GO_OR_QUIT(ReleasePipeline());

            if (nTimeout != 0)
            {
                reliabilityTimer.Stop();
                if (reliabilityTimer.OverallTiming() > nTimeout)
                    break;
            }
        }
    }
    catch(std::exception & ex)
    {
        vm_string_printf(VM_STRING("Exception: ")STR_TOKEN(), ex.what());
        vm_string_printf(VM_STRING("\nFAILED\n"));
        return 1;
    }
    catch(...)
    {
        vm_string_printf(VM_STRING("Unknown exception"));
        vm_string_printf(VM_STRING("\nFAILED\n"));
        return 1;
    }

    vm_string_printf(VM_STRING("\nPASS\n"));
    return 0;
}
