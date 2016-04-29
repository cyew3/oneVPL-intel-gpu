/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "mfxvideo.h"
#include <string>
#include <vector>


//general initializer interface
class IInitParams
{
public :
    virtual ~IInitParams(){}
};

enum RefListType
{
    REFLIST_PREFERRED,
    REFLIST_REJECTED,
    REFLIST_LONGTERM,
};

//pipeline interface mostly necessary for performing actions it
class IPipeline
{
public:
    virtual ~IPipeline(){}
    virtual mfxStatus Run() = 0;
    virtual mfxStatus Init(IInitParams *) = 0;
    virtual mfxStatus Close() = 0;
    virtual void      PrintInfo() = 0;
    //inserts 1 frameorder to specific framelist for current frame only
    virtual mfxStatus AddFrameToRefList(RefListType refList, mfxU32 nFrameOrder) = 0;
    //instructs pipeline to inser Key Frame at currently encoding frame
    virtual mfxStatus InsertKeyFrame() = 0;
    //encoder should change current bitrate
    virtual mfxStatus ScaleBitrate(double dScaleBy) = 0;
    //specify l0 size for current frame
    virtual mfxStatus SetNumL0Active(mfxU16 nActive) = 0;
    //setet pipeline using source information stored at index in initialisation parameters
    virtual mfxStatus ResetPipeline(int nSourceIdx) = 0;
};

//video conference actions for bitrate change, decoding error reporting interface, etc
class IAction
{
public:
    virtual ~IAction(){}
    virtual mfxStatus ApplyFeedBack(IPipeline *) = 0;
    //command invoked by manager until next idr
    virtual bool IsRepeatUntillIDR() = 0;
    //command is permanent, i.e. it will be invoked every frame after activating
    virtual bool IsPermanent() = 0;
};

//advanced container for actions
class IActionProcessor
{
public:
    virtual ~IActionProcessor(){}
    virtual void RegisterAction(mfxU32 nFrame, IAction *pAction) = 0;
    //received this notification when IDR produced by encoder
    virtual void OnFrameEncoded(mfxBitstream * pBs) = 0;
    virtual void GetActionsForFrame(mfxU32 nFrameOrder, std::vector<IAction*> & actions) = 0;
};

//proposed interface for external brc
class IBRC
{
public:
    virtual ~IBRC(){}
    //note that encoder should be initialized in CQP mode, so change encoder params after init BRC
    virtual mfxStatus Init(mfxVideoParam *pvParams) = 0;
    virtual void Update(mfxBitstream *pbs) = 0;
    virtual mfxStatus Reset(mfxVideoParam *pvParams) = 0;
    //current target bitrate
    virtual mfxU16 GetCurrentBitrate() = 0;
    virtual mfxU16 GetFrameQP() = 0;
    virtual mfxU16 GetFrameType() = 0;
};