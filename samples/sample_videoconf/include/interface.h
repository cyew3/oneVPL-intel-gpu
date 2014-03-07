/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

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