/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_SPL_PIPELINE_H
#define __MFX_SPL_PIPELINE_H

#include "mfx_ibitstream_reader.h"

namespace UMC
{
    class  Splitter;
    class  MediaData;
    class  SplitterInfo;
    struct sVideoStreamInfo;
    class  SplitterParams;
    class  DataReader;
}

class MFXFrameConstructor;

class UMCSplWrapper : public IBitstreamReader
{
    UMC::Splitter *         m_pSplitter;
    UMC::DataReader *       m_pReader;
    UMC::MediaData *        m_pInputData;
    UMC::SplitterInfo *     m_SplitterInfo;
    UMC::sVideoStreamInfo * m_pVideoInfo;
    int                     m_iVideoPid;
    int                     m_nFrames;
    mfxF64                  m_fLastTime;
    mfxF64                  m_fFirstTime;
    UMC::SplitterParams   * m_pInitParams;
    MFXFrameConstructor   * m_pConstructor;
    bool                    m_isVC1;
    //bool                    m_bTraceFrameType;
    bool                    m_bInited;
    //indicates whether inputdata consist of decspec info it is used to 
    //attach correct time stamp to it
    bool                    m_bDecSpecInfo;
    mfxU32                  m_nCorruptionLevel;

public:
    UMCSplWrapper(mfxU32 nCorruptionLevel = 0x0000);
    ~UMCSplWrapper();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual void      Close();
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &pBS);
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams);
    virtual mfxStatus SeekTime(mfxF64 fSeekTo);
    virtual mfxStatus SeekPercent(mfxF64 fSeekTo);
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info);
    virtual bool isFrameModeEnabled() {
        return true;
    }
protected:
    virtual mfxStatus GetCurrentTimeFromSpl();
    virtual mfxStatus SelectDataReader(const vm_char *strFileName);
};

#endif//__MFX_SPL_PIPELINE_H
