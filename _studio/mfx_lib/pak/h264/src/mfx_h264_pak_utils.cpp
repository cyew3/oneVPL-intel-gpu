//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include <assert.h>
#include "mfx_h264_pak.h"
#include "mfx_h264_pak_pred.h"
#include "mfx_h264_pak_utils.h"
#include "mfx_h264_pak_deblocking.h"

using namespace H264Pak;

enum
{
    MVPRED_MEDIAN,
    MVPRED_A,
    MVPRED_B,
    MVPRED_C
};

mfxU16 GetSliceId(const mfxFrameCUC& cuc, mfxU32 mbCnt)
{
    for (mfxU16 i = 0; i < cuc.NumSlice; i++)
    {
        const mfxFrameParamAVC& fp = cuc.FrameParam->AVC;
        const mfxSliceParamAVC& sp = cuc.SliceParam[i].AVC;
        mfxU32 firstMb = sp.FirstMbX + sp.FirstMbY * (fp.FrameWinMbMinus1 + 1);

        if (mbCnt >= firstMb && mbCnt < firstMb + sp.NumMb)
        {
            return i;
        }
    }

    assert("bad mb count");
    return cuc.NumSlice;
}

MbDescriptor::MbDescriptor(const mfxFrameCUC& cuc, mfxU8* mvdBuffer, mfxU32 mbNum, bool ignoreSliceBoundaries)
: parts(cuc.MbParam->Mb[mbNum].AVC)
, mbN(cuc.MbParam->Mb[mbNum].AVC)
{
    mfxExtAvcMvData* extMvs = (mfxExtAvcMvData *)GetExtBuffer(cuc, MFX_CUC_AVC_MV);
    mfxMbCode* mbs = cuc.MbParam->Mb;
    MbMvs* mvs = (MbMvs *)extMvs->Mv;
    MbMvs* mvd = (MbMvs *)mvdBuffer;

    mvN = &mvs[mbNum];
    mvdN = &mvd[mbNum];
    sliceIdN = GetSliceId(cuc, mbNum);

    mfxFrameParamAVC& fp = cuc.FrameParam->AVC;
    mfxSliceParamAVC& sp = cuc.SliceParam[sliceIdN].AVC;
    mfxU32 firstMbInSlice = sp.FirstMbY * (fp.FrameWinMbMinus1 + 1) + sp.FirstMbX;

    mbA = mbB = mbC = mbD = 0;
    mvA = mvB = mvC = mvD = 0;
    mvdA = mvdB = 0;
    sliceIdA = sliceIdB = 0;

    if (mbN.MbXcnt > 0 &&
        (ignoreSliceBoundaries || mbNum >= firstMbInSlice + 1))
    {
        mbA = &mbs[mbNum - 1].AVC;
        mvA = &mvs[mbNum - 1];
        mvdA = (mvd == 0) ? 0 : &mvd[mbNum - 1];
        sliceIdA = GetSliceId(cuc, mbNum - 1);
    }

    if (mbN.MbYcnt > 0 &&
        (ignoreSliceBoundaries || mbNum >= firstMbInSlice + fp.FrameWinMbMinus1 + 1))
    {
        mbB = &mbs[mbNum - fp.FrameWinMbMinus1 - 1].AVC;
        mvB = &mvs[mbNum - fp.FrameWinMbMinus1 - 1];
        mvdB = (mvd == 0) ? 0 : &mvd[mbNum - fp.FrameWinMbMinus1 - 1];
        sliceIdB = GetSliceId(cuc, mbNum - fp.FrameWinMbMinus1 - 1);
    }

    if (mbN.MbXcnt < fp.FrameWinMbMinus1 && mbN.MbYcnt > 0 &&
        (ignoreSliceBoundaries || mbNum >= firstMbInSlice + fp.FrameWinMbMinus1))
    {
        mbC = &mbs[mbNum - fp.FrameWinMbMinus1].AVC;
        mvC = &mvs[mbNum - fp.FrameWinMbMinus1];
    }

    if (mbN.MbXcnt > 0 && mbN.MbYcnt > 0 &&
        (ignoreSliceBoundaries || mbNum >= firstMbInSlice + fp.FrameWinMbMinus1 + 2))
    {
        mbD = &mbs[mbNum - fp.FrameWinMbMinus1 - 2].AVC;
        mvD = &mvs[mbNum - fp.FrameWinMbMinus1 - 2];
    }
}

MbPartitionIterator::MbPartitionIterator(const mfxMbCodeAVC& mb)
: m_numPart(0)
, noPartsLessThan8x8(true)
{
    if (mb.reserved2b == INTER_MB_MODE_16x16)
    {
        parts[m_numPart++] = Partition(0, 4, 4);
    }
    else if (mb.reserved2b == INTER_MB_MODE_16x8)
    {
        parts[m_numPart++] = Partition(0, 4, 2);
        parts[m_numPart++] = Partition(8, 4, 2);
    }
    else if (mb.reserved2b == INTER_MB_MODE_8x16)
    {
        parts[m_numPart++] = Partition(0, 2, 4);
        parts[m_numPart++] = Partition(4, 2, 4);
    }
    else // INTER_MB_MODE_8x8
    {
        for (mfxU32 part = 0; part < 4; part++)
        {
            mfxU32 shape = (mb.SubMbShape >> (2 * part)) & 0x3;

            if (shape == SUB_MB_SHAPE_8x8)
            {
                parts[m_numPart++] = Partition(4 * part, 2, 2);
            }
            else if (shape == SUB_MB_SHAPE_8x4)
            {
                noPartsLessThan8x8 = false;
                parts[m_numPart++] = Partition(4 * part + 0, 2, 1);
                parts[m_numPart++] = Partition(4 * part + 2, 2, 1);
            }
            else if (shape == SUB_MB_SHAPE_4x8)
            {
                noPartsLessThan8x8 = false;
                parts[m_numPart++] = Partition(4 * part + 0, 1, 2);
                parts[m_numPart++] = Partition(4 * part + 1, 1, 2);
            }
            else // SUB_MB_SHAPE_4x4
            {
                noPartsLessThan8x8 = false;
                parts[m_numPart++] = Partition(4 * part + 0, 1, 1);
                parts[m_numPart++] = Partition(4 * part + 1, 1, 1);
                parts[m_numPart++] = Partition(4 * part + 2, 1, 1);
                parts[m_numPart++] = Partition(4 * part + 3, 1, 1);
            }
        }
    }
}

mfxI16Pair H264Pak::GetMvPred(
    const MbDescriptor& neighbours,
    mfxU32 list,
    mfxU32 block,
    mfxU32 blockW,
    mfxU32 blockH)
{
    Block4x4 neighbourA = neighbours.GetLeft(block);
    Block4x4 neighbourB = neighbours.GetAbove(block);
    Block4x4 neighbourC = neighbours.GetAboveRight(block, blockW);

    mfxI16Pair mvA = neighbourA.mb && neighbourA.mb->IntraMbFlag == 0
        ? neighbourA.mv->mv[list][g_Geometry[neighbourA.block].offBlk4x4]
        : nullMv;

    mfxI16Pair mvB = neighbourB.mb && neighbourB.mb->IntraMbFlag == 0
        ? neighbourB.mv->mv[list][g_Geometry[neighbourB.block].offBlk4x4]
        : nullMv;

    mfxI16Pair mvC = neighbourC.mb && neighbourC.mb->IntraMbFlag == 0
        ? neighbourC.mv->mv[list][g_Geometry[neighbourC.block].offBlk4x4]
        : nullMv;

    mfxU8 refIdxA = neighbourA.mb && neighbourA.mb->IntraMbFlag == 0
        ? neighbourA.mb->RefPicSelect[list][neighbourA.block / 4]
        : -1;

    mfxU8 refIdxB = neighbourB.mb && neighbourB.mb->IntraMbFlag == 0
        ? neighbourB.mb->RefPicSelect[list][neighbourB.block / 4]
        : -1;

    mfxU8 refIdxC = neighbourC.mb && neighbourC.mb->IntraMbFlag == 0
        ? neighbourC.mb->RefPicSelect[list][neighbourC.block / 4]
        : -1;

    mfxU8 refIdxN = neighbours.mbN.RefPicSelect[list][block / 4];

    mfxU32 predType = MVPRED_MEDIAN;
    if (refIdxA == refIdxN && refIdxB != refIdxN && refIdxC != refIdxN)
    {
        predType = MVPRED_A;
    }
    else if (refIdxA != refIdxN && refIdxB == refIdxN && refIdxC != refIdxN)
    {
        predType = MVPRED_B;
    }
    else if (refIdxA != refIdxN && refIdxB != refIdxN && refIdxC == refIdxN)
    {
        predType = MVPRED_C;
    }

    if (blockW == 4 && blockH == 2)
    {
        // 16x8 is a special case
        if (block == 0 && refIdxB == refIdxN)
        {
            predType = MVPRED_B;
        }
        else if (block == 8 && refIdxA == refIdxN)
        {
            predType = MVPRED_A;
        }
    }
    else if (blockW == 2 && blockH == 4)
    {
        // 8x16 is a special case
        if (block == 0 && refIdxA == refIdxN)
        {
            predType = MVPRED_A;
        }
        else if (block == 4 && refIdxC == refIdxN)
        {
            predType = MVPRED_C;
        }
    }

    if (neighbourB.mb == 0 && neighbourC.mb == 0)
    {
        predType = MVPRED_A;
    }

    if (predType == MVPRED_A)
    {
        return mvA;
    }
    else if (predType == MVPRED_B)
    {
        return mvB;
    }
    else if (predType == MVPRED_C)
    {
        return mvC;
    }
    else // MVPRED_MEDIAN
    {
        mfxI16Pair median =
        {
            IPP_MIN(mvA.x, mvB.x) ^ IPP_MIN(mvA.x, mvC.x) ^ IPP_MIN(mvB.x, mvC.x),
            IPP_MIN(mvA.y, mvB.y) ^ IPP_MIN(mvA.y, mvC.y) ^ IPP_MIN(mvB.y, mvC.y)
        };

        return median;
    }
}

MbPool::MbPool(mfxFrameCUC& cuc, PakResources& res)
: m_cuc(cuc)
, m_coeffs((CoeffData *)res.m_coeffs)
, m_mbDone((volatile Ipp32u *)res.m_mbDone)
{
}

PredThread::PredThread()
: m_routine(0)
, m_onStop(false)
{
    m_onRun.Init(0, 0);
    m_onReady.Init(0, 0);
    m_thread.Create(PredThread::Callback, this);
}

PredThread::~PredThread()
{
    m_onStop = true;
    m_onRun.Set();
    m_thread.Close();
}

bool PredThread::IsValid()
{
    return m_thread.IsValid();
}

void PredThread::Run(ThreadRoutine& routine)
{
    m_routine = &routine;
    m_onRun.Set();
}

void PredThread::Wait()
{
    m_onReady.Wait();
}

mfxU32 VM_THREAD_CALLCONVENTION PredThread::Callback(void* p)
{
    PredThread& self = *((PredThread *)p);

    for (;;)
    {
        self.m_onRun.Wait();

        if (self.m_onStop == true)
        {
            return 0;
        }

        try
        {
            self.m_routine->Do();
        }
        catch(...)
        {
            assert(!"EXCEPTION in ThreadRoutine::Do");
        }

        self.m_onReady.Set();
    }
}

ThreadPredWaveFront::ThreadPredWaveFront(
    MbPool& pool,
    SeqScalingMatrices8x8& scaling8x8,
    bool doDeblocking)
: m_pool(pool)
, m_scaling8x8(scaling8x8)
, m_doDeblocking(doDeblocking)
{
}

void ThreadPredWaveFront::Do()
{
    mfxFrameParamAVC& fp = m_pool.m_cuc.FrameParam->AVC;
    mfxExtAvcRefFrameParam* refFrameParam = (mfxExtAvcRefFrameParam *)GetExtBuffer(m_pool.m_cuc, MFX_CUC_AVC_REFFRAME);
    mfxExtAvcRefPicInfo& refPicInfo = refFrameParam->PicInfo[0];
    mfxU32 widthInMb = fp.FrameWinMbMinus1 + 1;

    for (mfxU32 y = 0; y <= fp.FrameHinMbMinus1; y++)
    {
        if (m_pool.TryLockMb(y * widthInMb))
        {
            mfxU32 mbCnt = y * widthInMb;
            for (mfxU32 x = 0; x <= fp.FrameWinMbMinus1; x++, mbCnt++)
            {
                MbDescriptor mb(m_pool.m_cuc, 0, mbCnt);

                mfxExtAvcRefSliceInfo& refSlice = refPicInfo.SliceInfo[mb.sliceIdN];

                if (IsTrueIntra(mb.mbN))
                {
                    // mbA is always ready
                    if (mb.mbC)
                    {
                        while (!m_pool.IsMbReady(GetMbCnt(fp, *mb.mbC)));
                    }
                    else if (mb.mbB)
                    {
                        while (!m_pool.IsMbReady(GetMbCnt(fp, *mb.mbB)));
                    }
                    else if (mb.mbD)
                    {
                        while (!m_pool.IsMbReady(GetMbCnt(fp, *mb.mbD)));
                    }
                }

                PredictMb(
                    fp,
                    refSlice,
                    *m_pool.m_cuc.FrameSurface,
                    mb,
                    m_scaling8x8.m_seqScalingMatrix8x8,
                    m_scaling8x8.m_seqScalingMatrix8x8Inv,
                    m_pool.m_coeffs[mbCnt]);

                if (m_doDeblocking && fp.RefPicFlag && y > 0 && x > 0)
                {
                    // deblock left above macroblock
                    MbDescriptor mbToDeblock(m_pool.m_cuc, 0, mbCnt - (fp.FrameWinMbMinus1 + 1 + 1), true);
                    mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mbToDeblock.sliceIdN].AVC;
                    if (slice.DeblockDisableIdc == 0)
                    {
                        DeblockMb(fp, slice, refPicInfo, mbToDeblock, *m_pool.m_cuc.FrameSurface);
                    }

                    // when line is finished, deblock also above macroblock
                    if (x == fp.FrameWinMbMinus1)
                    {
                        MbDescriptor mbToDeblock(m_pool.m_cuc, 0, mbCnt - (fp.FrameWinMbMinus1 + 1), true);
                        mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mbToDeblock.sliceIdN].AVC;
                        if (slice.DeblockDisableIdc == 0)
                        {
                            DeblockMb(fp, slice, refPicInfo, mbToDeblock, *m_pool.m_cuc.FrameSurface);
                        }
                    }

                    // when last line, deblock also macroblock before left macroblock
                    if (y == fp.FrameHinMbMinus1)
                    {
                        if (x > 1)
                        {
                            MbDescriptor mbToDeblock(m_pool.m_cuc, 0, mbCnt - 2, true);
                            mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mbToDeblock.sliceIdN].AVC;
                            if (slice.DeblockDisableIdc == 0)
                            {
                                DeblockMb(fp, slice, refPicInfo, mbToDeblock, *m_pool.m_cuc.FrameSurface);
                            }
                        }

                        // when last macroblock, deblock also left and current macroblocks
                        if (x == fp.FrameWinMbMinus1)
                        {
                            if (x > 0)
                            {
                                MbDescriptor mbToDeblock(m_pool.m_cuc, 0, mbCnt - 1, true);
                                mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mbToDeblock.sliceIdN].AVC;
                                if (slice.DeblockDisableIdc == 0)
                                {
                                    DeblockMb(fp, slice, refPicInfo, mbToDeblock, *m_pool.m_cuc.FrameSurface);
                                }
                            }

                            MbDescriptor mbToDeblock(m_pool.m_cuc, 0, mbCnt, true);
                            mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mbToDeblock.sliceIdN].AVC;
                            if (slice.DeblockDisableIdc == 0)
                            {
                                DeblockMb(fp, slice, refPicInfo, mbToDeblock, *m_pool.m_cuc.FrameSurface);
                            }
                        }
                    }
                }

                m_pool.MarkMbReady(mbCnt);
            }
        }
    }
}

ThreadDeblocking::ThreadDeblocking(
    MbPool& pool)
: m_pool(pool)
{
}

void ThreadDeblocking::Do()
{
    mfxFrameParamAVC& fp = m_pool.m_cuc.FrameParam->AVC;
    mfxExtAvcRefFrameParam* refFrameParam = (mfxExtAvcRefFrameParam *)GetExtBuffer(m_pool.m_cuc, MFX_CUC_AVC_REFFRAME);
    mfxExtAvcRefPicInfo& refPicInfo = refFrameParam->PicInfo[0];

    for (mfxU32 y = 0; y <= fp.FrameHinMbMinus1; y++)
    {
        if (vm_interlocked_cas32(
                &m_pool.m_mbDone[y * (fp.FrameWinMbMinus1 + 1)],
                MbPool::MB_LOCKED,
                MbPool::MB_FREE) == MbPool::MB_FREE)
        {
            for (mfxU32 x = 0; x <= fp.FrameWinMbMinus1; x++)
            {
                mfxU32 mbCnt = y * (fp.FrameWinMbMinus1 + 1) + x;
                MbDescriptor mb(m_pool.m_cuc, 0, mbCnt, true);

                const mfxSliceParamAVC& slice = m_pool.m_cuc.SliceParam[mb.sliceIdN].AVC;

                if (slice.DeblockDisableIdc == 0)
                {
                    if (y > 0 && x < fp.FrameWinMbMinus1)
                    {
                        while (m_pool.m_mbDone[mbCnt - fp.FrameWinMbMinus1] != MbPool::MB_DONE);
                    }

                    DeblockMb(fp, slice, refPicInfo, mb, *m_pool.m_cuc.FrameSurface);
                }

                vm_interlocked_xchg32(&m_pool.m_mbDone[mbCnt], MbPool::MB_DONE);
            }
        }
    }
}

PredThreadPool::PredThreadPool()
: m_threads(0)
, m_numThread(0)
{
}

mfxStatus PredThreadPool::Create(mfxU32 numThread)
{
    PredThread* tmp = new PredThread[numThread];

    for (mfxU32 i = 0; i < numThread; i++)
    {
        if (!tmp[i].IsValid())
        {
            delete[] tmp;
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    delete[] m_threads;
    m_threads = tmp;
    m_numThread = numThread;
    return MFX_ERR_NONE;
}

void PredThreadPool::Run(ThreadRoutine& routine)
{
    for (mfxU32 i = 0; i < m_numThread; i++)
    {
        m_threads[i].Run(routine);
    }
}

void PredThreadPool::Wait()
{
    for (mfxU32 i = 0; i < m_numThread; i++)
    {
        m_threads[i].Wait();
    }
}

void PredThreadPool::Destroy()
{
    delete[] m_threads;
    m_threads = 0;
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODER
