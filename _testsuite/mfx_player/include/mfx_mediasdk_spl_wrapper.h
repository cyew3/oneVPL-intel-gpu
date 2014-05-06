/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_MEDIASDK_SPL_PIPELINE_H
#define __MFX_MEDIASDK_SPL_PIPELINE_H

#include "mfx_ibitstream_reader.h"
#include "mfxsplmux.h"

class MFXFrameConstructor;

class MediaSDKSplWrapper : public IBitstreamReader
{
    struct  DataReader
    {
        vm_file* m_fSource;
        bool m_bInited;
    };

    DataReader              m_splReader;
    mfxDataIO               m_dataIO;
    mfxSplitter             m_mfxSplitter;
    mfxStreamParams         m_streamParams;
    MFXFrameConstructor*    m_pConstructor;
    vm_file*                m_extractedAudioFile;

public:
    MediaSDKSplWrapper(vm_char *extractedAudioFile);
    ~MediaSDKSplWrapper();
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

    static mfxI32 RdRead(void* in_DataReader, mfxBitstream *BS);
    static mfxI64 RdSeek(void* in_DataReader, mfxI64 offset, mfxSeekOrigin origin);
};

#endif//__MFX_MEDIASDK_SPL_PIPELINE_H
