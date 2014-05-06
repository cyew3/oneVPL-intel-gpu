/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: test_h264_decode_utils.h

\* ****************************************************************************** */

#ifndef __TEST_H264_DECODE_UTILS_H__
#define __TEST_H264_DECODE_UTILS_H__
#include <stdio.h>
#include "vm_strings.h"
#include "vm_file.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

#include "mfx_h264_ex_param_buf.h"
#include "umc_media_data_ex.h"

#define MFX_REPORT_ERROR_AND_EXIT(MSG) { vm_string_printf(VM_STRING(MSG)); return 1; }
#define MFX_CHECK_ERROR(STATUS, MSG) if (STATUS != MFX_ERR_NONE) MFX_REPORT_ERROR_AND_EXIT(MSG)

struct MFXHolder
{
    explicit MFXHolder(mfxIMPL impl)
    : mfx(0)
    {
        MFXInit(impl, 0, &mfx);
    }

    ~MFXHolder()
    {
        MFXClose(mfx);
        mfx = 0;
    }

    mfxSession mfx;
};

class MfxBitstreamHolder
{
public:
    explicit MfxBitstreamHolder(const vm_char* name = 0);
    ~MfxBitstreamHolder();
    bool IsOpen() const;
    bool IsEos() const;
    bool Open(const vm_char* name);
    bool Update();
    mfxBitstream& Extract();
    const mfxBitstream& Extract() const;
    bool Cache();

protected:
    mfxBitstream m_bs;
    static const Ipp32u m_bufSize = 50 * 1024;
    vm_file* m_file;
    Ipp32u m_dataLen;
    Ipp8u m_buf[m_bufSize];
    bool m_isEos;
};

class MfxFrameCucHolder
{
public:
    MfxFrameCucHolder();
    ~MfxFrameCucHolder();
    mfxFrameCUC& Extract();
    const mfxFrameCUC& Extract() const;
    void Init();
    void Close();

    void Reset();

    void AllocateFrameSurface(const mfxVideoParam &videoPar);
    void AllocSlicesAndMbs(mfxU32 numMbs);
    void AllocExtBuffers(const mfxFrameInfo& info, mfxU32 pitch);
    void FreeSlicesAndMbs();
    void FreeExtBuffers();
    void FreeFrameSurface();
    mfxStatus FreeExtBuffer(mfxU32 extBufferId);
protected:
    mfxFrameCUC m_cuc;
    static const mfxU32 NumExtBuffer = 6;
    mfxFrameSurface m_fs;
};

class Mfxh264SurfaceHolder
{
public:
    enum { MfxNoLabel = 0xff };
    enum { PicNone = 0, PicTop = 1, PicBot = 2, PicFrame = 3 };
    enum { PicN = 0, PicI = 1, PicP = 2, PicB = 3 };

public:
    Mfxh264SurfaceHolder();
    ~Mfxh264SurfaceHolder();
    mfxFrameSurface& Extract();
    const mfxFrameSurface& Extract() const;
    mfxFrameData& Data(mfxU32 frameLabel);
    const mfxFrameData& Data(mfxU32 frameLabel) const;
    mfxStatus Init(mfxU32 numFrameData, mfxU32 chromaFormat, mfxU32 width, mfxU32 height);
    mfxStatus Close();

    void PopDisplayable();
    mfxU8 Displayable() const;
    mfxU8 PushPic(const mfxFrameParam& fPar);
    void PushEos();
    mfxU8 FwdRef() const;
    mfxU8 BwdRef() const;

protected:
    mfxU8 PushPic(mfxU32 FrameType, mfxU32 FieldPicFlag, mfxU32 BottomFieldFlag);
    bool IsNewPic(mfxU32 FrameType, mfxU32 FieldPicFlag, mfxU32 BottomFieldFlag) const;
    mfxU8 FindAndLockFreeSurface() const;
    mfxU8 FindSurface(mfxU16 frameOrder) const;
    void SetFrameOrder(mfxU8 label);

    mfxFrameSurface m_fs;
    mfxU16 m_nextFrameOrder;
    mfxU16 m_firstDispFrameOrder;
    mfxU8 m_firstDispLabel;

    mfxU8 m_type;
    mfxU8 m_fullness;
    mfxU8 m_labelCur;
    mfxU8 m_labelPrev;
    mfxU8 m_labelPrevPrev;
    bool m_bEos;
};

inline static mfxStatus ConvertMfxBSToMediaData(mfxBitstream *pBitstream, UMC::MediaData * data)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    data->SetBufferPointer(pBitstream->Data, pBitstream->MaxLength);
    data->SetDataSize(pBitstream->DataOffset + pBitstream->DataLength);
    data->MoveDataPointer(pBitstream->DataOffset);
    return MFXSts;
}

#endif
