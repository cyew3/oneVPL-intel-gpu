/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_PIPLINE_H
#define __MFX_PIPLINE_H

#include <mfxstructures.h>
#include "mfx_ipipeline_sync.h"
#include "vm_strings.h"

class IMFXPipeline
{
public:
    virtual ~IMFXPipeline(){}
    //part of old IPlayback interface
    virtual mfxStatus  ProcessCommand( vm_char ** &argv
                                     , mfxI32      argc
                                     , bool        bReportError = true) = 0;

    //expiremental feature to obtain parameters from par files
    virtual mfxStatus   ReconstructInput( vm_char ** argv
                                        , mfxI32    argc
                                        , void * /*std::vector<tstring> *pReconstrcutedArgs*/) = 0;
    //
    virtual int        PrintHelp() = 0;

    //checks for input parameters
    virtual mfxStatus   BuildPipeline()= 0;
    virtual mfxStatus   ReleasePipeline() = 0;
    //reseting mfx part of pipeline
    virtual mfxStatus   LightReset()=0;
    virtual mfxStatus   HeavyReset()=0;

    //only function from large playback interface
    virtual mfxStatus   Play() = 0;

    //returns error description
    virtual vm_char *   GetLastErrString() = 0;

    //returns multiple&reliability values
    virtual mfxStatus   GetMulTipleAndReliabilitySettings(mfxU32 &nRepeat, mfxU32 &nTimeout) = 0;
    // return allowability of resetting after gpu hang
    virtual mfxStatus   GetGPUErrorHandlingSettings(bool &bHeavyResetAllowed) = 0;

    //externally sets ref file
    virtual mfxStatus   SetRefFile(const vm_char * pRefFile, mfxU32 nDelta) = 0;
    //externally gets out file
    virtual mfxStatus   GetOutFile(vm_char * ppOutFile, int bufsize)        = 0;
    //externally sets out file
    virtual mfxStatus   SetOutFile(const vm_char * pOutFile)                = 0;
    //returns whether memory usage could be reduced and reduce it. Pipeline /release / build required
    virtual mfxStatus   ReduceMemoryUsage()                               = 0;
    //instruct pipeline to use external synhro object to share access to non thread safety APIs
    virtual mfxStatus   SetSyncro(IPipelineSynhro * pSynchro)             = 0;
};

#endif//__MFX_PIPLINE_H
