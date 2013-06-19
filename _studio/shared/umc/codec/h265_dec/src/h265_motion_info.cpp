/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs_dec.h"
#include <memory>
#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "h265_motion_info.h"

namespace UMC_HEVC_DECODER
{

#if !(HEVC_OPT_CHANGES & 2)
// ML: OPT: moved into the header to allow inlining
Ipp32s H265MotionVector::getAbsHor() const
{
    return abs(Horizontal);
}

Ipp32s H265MotionVector::getAbsVer() const
{
    return abs(Vertical);
}
#endif

void CUMVBuffer::create(Ipp32u numPartition )
{
    VM_ASSERT(MV == NULL);
    VM_ASSERT(MVd == NULL);
    VM_ASSERT(RefIdx == NULL);

    MV = new H265MotionVector[numPartition];
    MVd = new H265MotionVector[numPartition];
    RefIdx = new RefIndexType[numPartition];

    m_NumPartition = numPartition;
}

void CUMVBuffer::destroy()
{
    VM_ASSERT(MV != NULL);
    VM_ASSERT(MVd != NULL);
    VM_ASSERT(RefIdx != NULL);

    delete[] MV;
    delete[] MVd;
    delete[] RefIdx;

    MV = NULL;
    MVd = NULL;
    RefIdx = NULL;

    m_NumPartition = 0;
}

void CUMVBuffer::clearMVBuffer()
{
#if (HEVC_OPT_CHANGES & 4)
    memset( MV, 0, sizeof( *MV ) * m_NumPartition );
    memset( MVd, 0, sizeof( *MVd ) * m_NumPartition );
#else
    for (Ipp32u i = 0; i < m_NumPartition; i++)
    {
        MV[i].setZero();
        MVd[i].setZero();
    }
#endif
    VM_ASSERT(sizeof(*RefIdx) == 1);
    memset(RefIdx, NOT_VALID, m_NumPartition * sizeof(*RefIdx));
}

void CUMVBuffer::copyFrom(CUMVBuffer const * CUMVBufferSrc, Ipp32s NumPartSrc, Ipp32s PartAddrDst)
{
    Ipp32s SizeInMV = sizeof(H265MotionVector) * NumPartSrc;

    memcpy(MV + PartAddrDst, CUMVBufferSrc->MV, SizeInMV);
    memcpy(MVd + PartAddrDst, CUMVBufferSrc->MVd, SizeInMV);
    memcpy(RefIdx + PartAddrDst, CUMVBufferSrc->RefIdx, sizeof(*RefIdx) * NumPartSrc);
}

void CUMVBuffer::copyTo(CUMVBuffer* CUMVBufferDst, Ipp32s PartAddrDst) const
{
    copyTo(CUMVBufferDst, PartAddrDst, 0, m_NumPartition);
}

void CUMVBuffer::copyTo(CUMVBuffer* CUMVBufferDst, Ipp32s PartAddrDst, Ipp32u offset, Ipp32u NumPart) const
{
    Ipp32s SizeInMV = sizeof(H265MotionVector) * NumPart;
    Ipp32s Offset = offset + PartAddrDst;

    memcpy(CUMVBufferDst->MV + Offset, MV + offset, SizeInMV);
    memcpy(CUMVBufferDst->MVd + Offset, MVd + offset, SizeInMV);
    memcpy(CUMVBufferDst->RefIdx + Offset, RefIdx + offset, sizeof(*RefIdx) * NumPart);
}

// set
template <typename T>
void CUMVBuffer::setAll(T *p, T const & val, EnumPartSize CUMode, Ipp32s PartAddr, Ipp32u Depth, Ipp32s PartIdx)
{
    Ipp32s i;
    p += PartAddr;
    Ipp32s numElements = m_NumPartition >> (2 * Depth);

    switch(CUMode)
    {
        case SIZE_2Nx2N:
            for (i = 0; i < numElements; i++ )
            {
                p[i] = val;
            }
            break;

        case SIZE_2NxN:
            numElements >>= 1;
            for (i = 0; i < numElements; i++)
            {
                p[i] = val;
            }
            break;

        case SIZE_Nx2N:
            numElements >>= 2;
            for (i = 0; i < numElements; i++)
            {
                p[i] = val;
                p[i + 2 * numElements] = val;
            }
            break;

        case SIZE_NxN:
            numElements >>= 2;
            for (i = 0; i < numElements; i++)
            {
                p[i] = val;
            }
            break;

        case SIZE_2NxnU:
        {
            Ipp32s CurrPartNumQ = numElements >> 2;
            if (PartIdx == 0)
            {
                T *pT  = p;
                T *pT2 = p + CurrPartNumQ;
                for (i = 0; i < (CurrPartNumQ >> 1); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }
            }
            else
            {
                T *pT  = p;
                for (i = 0; i < (CurrPartNumQ >> 1); i++)
                {
                    pT[i] = val;
                }

                pT = p + CurrPartNumQ;
                for (i = 0; i < ((CurrPartNumQ >> 1) + (CurrPartNumQ << 1)); i++)
                {
                    pT[i] = val;
                }
            }
            break;
        }
        case SIZE_2NxnD:
        {
            Ipp32s CurrPartNumQ = numElements >> 2;
            if (PartIdx == 0)
            {
                T *pT  = p;
                for (i = 0; i < ((CurrPartNumQ >> 1) + (CurrPartNumQ << 1)); i++)
                {
                    pT[i] = val;
                }
                pT = p + (numElements - CurrPartNumQ);
                for (i = 0; i < (CurrPartNumQ >> 1); i++)
                {
                    pT[i] = val;
                }
            }
            else
            {
                T *pT  = p;
                T *pT2 = p + CurrPartNumQ;
                for (i = 0; i < (CurrPartNumQ >> 1); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }
            }
            break;
        }
        case SIZE_nLx2N:
        {
            Ipp32s CurrPartNumQ = numElements >> 2;
            if (PartIdx == 0)
            {
                T *pT  = p;
                T *pT2 = p + (CurrPartNumQ << 1);
                T *pT3 = p + (CurrPartNumQ >> 1);
                T *pT4 = p + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1);

                for (i = 0; i < (CurrPartNumQ >> 2); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                    pT3[i] = val;
                    pT4[i] = val;
                }
            }
            else
            {
                T *pT  = p;
                T *pT2 = p + (CurrPartNumQ << 1);
                for (i = 0; i < (CurrPartNumQ >> 2); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }

                pT  = p + (CurrPartNumQ >> 1);
                pT2 = p + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1);
                for (i = 0; i < ((CurrPartNumQ >> 2) + CurrPartNumQ); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }
            }
            break;
        }
        case SIZE_nRx2N:
        {
            Ipp32s CurrPartNumQ = numElements >> 2;
            if (PartIdx == 0)
            {
                T *pT  = p;
                T *pT2 = p + (CurrPartNumQ << 1);
                for (i = 0; i < ((CurrPartNumQ >> 2) + CurrPartNumQ); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }

                pT  = p + CurrPartNumQ + (CurrPartNumQ >> 1);
                pT2 = p + numElements - CurrPartNumQ + (CurrPartNumQ >> 1);
                for (i = 0; i < (CurrPartNumQ >> 2); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                }
            }
            else
            {
                T *pT  = p;
                T *pT2 = p + (CurrPartNumQ >> 1);
                T *pT3 = p + (CurrPartNumQ << 1);
                T *pT4 = p + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1);
                for (i = 0; i < (CurrPartNumQ >> 2); i++)
                {
                    pT [i] = val;
                    pT2[i] = val;
                    pT3[i] = val;
                    pT4[i] = val;
                }
            }
            break;
        }
        default:
            VM_ASSERT(0);
            break;
  }
};

void CUMVBuffer::setAllMV(H265MotionVector const & mv, EnumPartSize CUMode, Ipp32s PartAddr, Ipp32u Depth, Ipp32s PartIdx)
{
    setAll(MV, mv, CUMode, PartAddr, Depth, PartIdx);
}

void CUMVBuffer::setAllMVd(H265MotionVector const & mvd, EnumPartSize CUMode, Ipp32s PartAddr, Ipp32u Depth, Ipp32s PartIdx)
{
    setAll(MVd, mvd, CUMode, PartAddr, Depth, PartIdx);
}

void CUMVBuffer::setAllRefIdx (Ipp32s refIdx, EnumPartSize CUMode, Ipp32s PartAddr, Ipp32u Depth, Ipp32s PartIdx)
{
    setAll(RefIdx, static_cast<RefIndexType>(refIdx), CUMode, PartAddr, Depth, PartIdx);
}

void CUMVBuffer::setAllMVBuffer(MVBuffer const & mvBuffer, EnumPartSize CUMode, Ipp32s PartAddr, Ipp32u Depth, Ipp32s PartIdx)
{
    setAllMV (mvBuffer.MV, CUMode, PartAddr, Depth, PartIdx);
    setAllRefIdx (mvBuffer.RefIdx, CUMode, PartAddr, Depth, PartIdx);
}
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
