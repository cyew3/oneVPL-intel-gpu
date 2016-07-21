/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec_common.h

\* ****************************************************************************** */

#ifndef __MFX_MPEG2_DEC_COMMON_H__
#define __MFX_MPEG2_DEC_COMMON_H__

#include "mfx_common.h"
#if defined MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "mfx_common_int.h"
#include "mfxvideo.h"
#include "umc_mpeg2_dec_defs.h"

#define MFX_PROFILE_MPEG1 8

enum { MFX_MB_DATA_SIZE_ALIGNMENT = 128 / 8 };
enum { MFX_MB_DATA_OFFSET_ALIGNMENT = 32 / 8 };
enum { MFX_MB_WIDTH = 16 };
enum { MFX_MB_HEIGHT = 16 };

enum
{
    PB_Skip_L0 = 0x01,
    B_Skip_L1 = 0x02,
    B_Skip_Bi = 0x03,
    PB_16x16_L0 = 0x01,
    B_16x16_L1 = 0x02,
    B_16x16_Bi = 0x03,
    PB_Skip_Field_L0L0 = 0x04,
    B_Skip_Field_L1L1 = 0x06,
    B_Skip_Field_L2L2 = 0x14,
    PB_16x8_Field_L0L0 = 0x04,
    B_16x8_Field_L1L1 = 0x06,
    B_16x8_Field_L2L2 = 0x14,
    P_DualPrime = 0x19,
    MPEG2_Intra = 0x1a,
    MPEG2_Intra_Field = 0x1a
};

static const mfxU16 MFX_MPEG2_MAX_WIDTH = (1 << (12 + 2)) - 1; // 12 bits + 2 bits of extension
static const mfxU16 MFX_MPEG2_MAX_HEIGHT = (1 << (12 + 2)) - 1; // 12 bits + 2 bits of extension

static const mfxU16 MFX_MAX_WIDTH_HEIGHT = 4080;

enum
{
    MFX_MPEG2_MAX_FR_EXT_N = (1 << 2) - 1, // 2 bit integer
    MFX_MPEG2_MAX_FR_EXT_D = (1 << 5) - 1  // 5 bit integer
};

void ConvertBitStreamMfx2Umc(const mfxBitstream& mfxBs, UMC::Mpeg2Bitstream& umcBs);
void ConvertBitStreamUmc2Mfx(const UMC::Mpeg2Bitstream& umcBs, mfxBitstream& mfxBs);
mfxU8 GetMfxSecondFieldFlag(mfxU32 curPicStruct, mfxU32 prevFieldPicFlag, mfxU32 prevBottomFieldFlag, mfxU32 prevSecondFieldFlag);
mfxU8 GetMfxFrameType(Ipp8u picType, Ipp32u picStruct);
void AddPredAndResidual(mfxU8*, mfxU32, mfxU8*, mfxU32, mfxI16*, mfxU32, mfxU32, mfxU32);
mfxMbCode* GetFirstMfxMbCode(const mfxFrameCUC &cuc);
mfxI16* GetDctCoefPtr(mfxHDL pExtBufResCoef, mfxU32 mbX, mfxU32 mbY, const mfxFrameParam& par);
mfxU8 GetMfxNumRefFrame(Ipp8u frameType);
mfxU16 GetMfxBitStreamFcodes(Ipp8u *fCode);
//mfxU16 GetMfxFrameRateCode(mfxU8 umcFrameRateCode);
void GetMfxFrameRate(mfxU8 umcFrameRateCode, mfxU32 *frameRateNom, mfxU32 *frameRateDenom);
void Mpeg2SetConfigurableCommon(mfxVideoParam &par);
void Mpeg2CheckConfigurableCommon(mfxVideoParam &par);
mfxU8* GetPlane(const mfxFrameSurface& fs, mfxU32 data, mfxU32 plane);
mfxU16 GetPitch(const mfxFrameSurface& fs, mfxU32 data, mfxU32 plane);
inline bool IsMpeg2StartCodeEx(const mfxU8* p);

inline
mfxU8 Mpeg2GetMfxChromaFormat(mfxU32 chromaFormatIdc)
{
    return (mfxU8)(chromaFormatIdc + 1);
}

template<class T> inline
T* GetExtBuffer(mfxFrameCUC& cuc, mfxU32 id)
{
        for (mfxU32 i = 0; i < cuc.NumExtBuffer; i++)
            if (cuc.ExtBuffer[i] != 0 && ((mfxU32 *)cuc.ExtBuffer[i])[0] == id) // assuming aligned buffers
                return (T *)(((mfxU32 *)cuc.ExtBuffer[i]) + 0);
    return 0;
}

inline
bool IsAligned(mfxU32 value, mfxU32 divisor)
{
    return (value == (value / divisor) * divisor);
}

template<class T> inline
T AlignValue(T value, mfxU32 divisor)
{
    return static_cast<T>(((value + divisor - 1) / divisor) * divisor);
}

template<class T> inline
T AlignValueSat(T value, mfxU32 divisor)
{
    T maxValue = static_cast<T>((-1 / divisor) * divisor);
    return value > maxValue ? maxValue : AlignValue(value, divisor);
}

class MemLocker
{
public:
    MemLocker(VideoCORE& core, bool lockable)
    : m_core(core)
    , m_lockable(lockable)
    {
    }

protected:
    VideoCORE& m_core;
    bool m_lockable;

private:
    MemLocker(const MemLocker&);
    MemLocker& operator=(const MemLocker&);
};

class BufferLocker : public MemLocker
{
public:
    template <class T>
    BufferLocker(VideoCORE& core, T& ptr, mfxHDL mid)
    : MemLocker(core, ptr == 0)
    , m_ptr(static_cast<mfxU8*&>(ptr))
    , m_mid(mid)
    {
        if (m_lockable)
            m_lockable = (m_core.LockBuffer(m_mid, &m_ptr) == MFX_ERR_NONE);
    }

    BufferLocker(VideoCORE& core, mfxBitstream& bs)
    : MemLocker(core, bs.Data == 0)
    , m_ptr(bs.Data)
    , m_mid(0/*bs.MemId*/)
    {
        if (m_lockable)
            m_lockable = (m_core.LockBuffer(m_mid, &m_ptr) == MFX_ERR_NONE);
    }


    ~BufferLocker()
    {
        if (m_lockable)
            m_core.UnlockBuffer(m_mid/*, &m_ptr*/);
    }

private:
    mfxU8*& m_ptr;
    mfxHDL m_mid;
};

class FrameLocker : public MemLocker
{
public:
    FrameLocker(VideoCORE& core, mfxFrameData& frame)
    : MemLocker(core, frame.Y == 0 && frame.U == 0 && frame.V == 0 && frame.A == 0)
    , m_frame(frame)
    {
        if (m_lockable)
            m_lockable = (m_core.LockFrame(m_frame.MemId, &m_frame) == MFX_ERR_NONE);
    }

    ~FrameLocker()
    {
        if (m_lockable)
            m_core.UnlockFrame(m_frame.MemId, &m_frame);
    }

private:
    mfxFrameData& m_frame;
};

#endif

#endif //__MFX_MPEG2_DEC_COMMON_H__
