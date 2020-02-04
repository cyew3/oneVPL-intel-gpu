// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#include "hevcehw_g9_dump_files.h"

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && (DEBUG_REC_FRAMES_INFO)

using namespace HEVCEHW;
using namespace HEVCEHW::Gen9;

DumpFiles::~DumpFiles()
{
    vm_file_fclose(m_dumpRaw);
    vm_file_fclose(m_dumpRec);
}

void DumpFiles::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_DUMP].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& src = *(const mfxExtDumpFiles*)pSrc;
        auto& dst = *(mfxExtDumpFiles*)pDst;
        dst = src;
    });
}

void DumpFiles::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageW& strg, StorageW&) -> mfxStatus
    {
        const mfxExtDumpFiles* pDF = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        MFX_CHECK(pDF, MFX_ERR_NONE);

        bool bOpen = !strg.Contains(Glob::ResetHint::Key)
            || (Glob::ResetHint::Get(strg).Flags & RF_IDR_REQUIRED);

        if (bOpen)
        {
            vm_file_fclose(m_dumpRaw);
            vm_file_fclose(m_dumpRec);

            m_dumpRaw = vm_file_fopen(pDF->InputFramesFilename, VM_STRING("wb"));
            m_dumpRec = vm_file_fopen(pDF->ReconFilename, VM_STRING("wb"));
        }

        return MFX_ERR_NONE;
    });
}

mfxStatus WriteFrameData(
    vm_file *            file,
    mfxFrameData const & fdata,
    mfxFrameInfo const   info)
{
    MFX_CHECK_NULL_PTR1(file);

    mfxFrameData data = fdata;
    mfxU32 pitch = data.PitchLow + ((mfxU32)data.PitchHigh << 16);

    auto WriteNV12asI420 = [&]()
    {
        MFX_CHECK_NULL_PTR2(data.Y, data.UV);
        mfxU32 planeSize = info.Width * info.Height;
        std::vector<mfxU8> frame(planeSize * 3 / 2);
        auto pData = frame.data();
        mfxU32 rowSize = info.Width;

        std::for_each(
            MakeStepIter(data.Y, pitch)
            , MakeStepIter(data.Y + info.Height * pitch, pitch)
            , [&](mfxU8& rRow)
        {
            std::copy_n(&rRow, rowSize, pData);
            pData += rowSize;
        });

        rowSize /= 2;
        std::for_each(
            MakeStepIter(data.UV, pitch)
            , MakeStepIter(data.UV + info.Height / 2 * pitch, pitch)
            , [&](mfxU8& rRow)
        {
            std::copy_n(MakeStepIter(&rRow, 2), rowSize, pData);
            std::copy_n(MakeStepIter(&rRow + 1, 2), rowSize, pData + planeSize / 4);
            pData += rowSize;
        });

        auto nWritten =vm_file_fwrite(frame.data(), 1, (int)frame.size(), file);
        MFX_CHECK(nWritten == frame.size(), MFX_ERR_UNKNOWN);

        return MFX_ERR_NONE;
    };
    auto WriteAYUVasI444 = [&]()
    {
        MFX_CHECK_NULL_PTR3(data.V, data.U, data.Y);
        mfxU32 planeSize = info.Width * info.Height;
        std::vector<mfxU8> frame(planeSize * 3);
        auto pY = frame.data();
        auto pU = pY + planeSize;
        auto pV = pU + planeSize;
        mfxU32 rowSize = info.Width;
        mfxU32 offV = 0, offU = 1, offY = 2;

        std::for_each(
            MakeStepIter(data.V, pitch)
            , MakeStepIter(data.V + info.Height * pitch, pitch)
            , [&](mfxU8& rRow)
        {
            std::copy_n(MakeStepIter(&rRow + offY, 4), rowSize, pY);
            pY += rowSize;
            std::copy_n(MakeStepIter(&rRow + offU, 4), rowSize, pU);
            pU += rowSize;
            std::copy_n(MakeStepIter(&rRow + offV, 4), rowSize, pV);
            pV += rowSize;
        });

        auto nWritten = vm_file_fwrite(frame.data(), 1, (int)frame.size(), file);
        MFX_CHECK(nWritten == frame.size(), MFX_ERR_UNKNOWN);

        return MFX_ERR_NONE;
    };
    auto WriteY410asI410 = [&]()
    {
        MFX_CHECK_NULL_PTR1(data.Y410);
        mfxU32 planeSize = info.Width * info.Height;
        std::vector<mfxU16> frame(planeSize * 3);
        auto pY = frame.data();
        auto pU = pY + planeSize;
        auto pV = pU + planeSize;
        mfxU32 rowSize = info.Width;

        pitch /= sizeof(data.Y410);

        std::for_each(
            MakeStepIter(data.Y410, pitch)
            , MakeStepIter(data.Y410 + info.Height * pitch, pitch)
            , [&](mfxY410& rRow)
        {
            std::transform(&rRow, &rRow + rowSize, pY, [](mfxY410 p) { return (mfxU16)p.Y; });
            std::transform(&rRow, &rRow + rowSize, pU, [](mfxY410 p) { return (mfxU16)p.U; });
            std::transform(&rRow, &rRow + rowSize, pV, [](mfxY410 p) { return (mfxU16)p.V; });
        });

        auto nWritten = vm_file_fwrite(frame.data(), 2, (int)frame.size(), file);
        MFX_CHECK(nWritten == frame.size(), MFX_ERR_UNKNOWN);

        return MFX_ERR_NONE;
    };

    switch (info.FourCC)
    {
    case MFX_FOURCC_NV12: return WriteNV12asI420();
    case MFX_FOURCC_AYUV: return WriteAYUVasI444();
    case MFX_FOURCC_Y410: return WriteY410asI410();
    default: return MFX_ERR_UNSUPPORTED;
    }
}

void DumpFiles::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_DumpRaw
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_dumpRaw, MFX_ERR_NONE);

        auto& core = Glob::VideoCore::Get(global);
        auto& task = Task::Common::Get(s_task);
        bool bExternal = !task.Raw.Mid;

        FrameLocker data(
            core
            , bExternal ? task.pSurfReal->Data.MemId : task.Raw.Mid
            , bExternal);

        return WriteFrameData(m_dumpRaw, data, Glob::VideoParam::Get(global).mfx.FrameInfo);
    });
    Push(BLK_DumpRec
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_dumpRec, MFX_ERR_NONE);

        auto& core = Glob::VideoCore::Get(global);
        auto& task = Task::Common::Get(s_task);
        FrameLocker data(core, task.Rec.Mid);

        return WriteFrameData(m_dumpRec, data, Glob::AllocRec::Get(global).GetInfo());
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)