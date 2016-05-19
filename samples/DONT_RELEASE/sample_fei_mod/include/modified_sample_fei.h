/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once
#include "mfxfei.h"
#include "pipeline_fei.h"

mfxStatus PakOneStreamoutFrame(mfxU32 m_numOfFields, iTask *eTask, mfxU8 QP, std::list<iTask*> *pTaskList);
mfxStatus RepackStremoutMB2PakMB(mfxFeiDecStreamOutMBCtrl* dsoMB, mfxFeiPakMBCtrl* pakMB, mfxU8 QP);
mfxStatus RepackStreamoutMV(mfxFeiDecStreamOutMBCtrl* dsoMB, mfxExtFeiEncMV::mfxExtFeiEncMVMB* encMB);
void checkMBs(mfxFeiPakMBCtrl* encMB, mfxFeiPakMBCtrl* in_encMB);

mfxStatus ResetDirect (struct iTask * task, std::list<iTask*> *pTaskList);
