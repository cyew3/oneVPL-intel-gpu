/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "ipps.h"
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_frame.h"
#include "umc_h264_slice_ex.h"

namespace UMC
{
FactorArrayAFF H264SliceEx::m_StaticFactorArrayAFF;

H264SliceEx::H264SliceEx(MemoryAllocator *pMemoryAllocator)
    : H264Slice(pMemoryAllocator)
    , m_coeffsBuffers(0)
{
    H264Slice::Reset();
}

void H264SliceEx::ZeroedMembers()
{
    H264Slice::ZeroedMembers();
    m_mbinfo = 0;

    m_DistScaleFactorAFF = 0;
    m_DistScaleFactorMVAFF = 0;

    m_bInited = false;
}

H264CoeffsBuffer *H264SliceEx::GetCoeffsBuffers() const
{
    return m_coeffsBuffers;
}

void H264SliceEx::FreeResources()
{
    if (!GetCoeffsBuffers())
        return;

    m_coeffsBuffers->DecrementReference();
    m_coeffsBuffers = 0;
}

void H264SliceEx::Release()
{
    if (GetCoeffsBuffers())
        m_coeffsBuffers->Reset();

    m_pObjHeap->FreeObject(m_DistScaleFactorAFF);
    m_pObjHeap->FreeObject(m_DistScaleFactorMVAFF);

    H264Slice::Release();
}

void H264SliceEx::InitializeContexts()
{
    if (m_isInitialized)
        return;

    m_SliceDataBitStream = m_BitStream;
    if (m_pPicParamSet->entropy_coding_mode)
    {
        // reset CABAC engine
        
        m_SliceDataBitStream.InitializeDecodingEngine_CABAC();
        if (INTRASLICE == m_SliceHeader.slice_type)
        {
            m_SliceDataBitStream.InitializeContextVariablesIntra_CABAC(m_pPicParamSet->pic_init_qp +
                                                              m_SliceHeader.slice_qp_delta);
        }
        else
        {
            m_SliceDataBitStream.InitializeContextVariablesInter_CABAC(m_pPicParamSet->pic_init_qp +
                                                              GetSliceHeader()->slice_qp_delta,
                                                              GetSliceHeader()->cabac_init_idc);
        }
    }

    m_isInitialized = true;
}

bool H264SliceEx::GetDeblockingCondition(void) const
{
    // there is no deblocking
    if (DEBLOCK_FILTER_OFF == GetSliceHeader()->disable_deblocking_filter_idc)
        return false;

    // no filtering edges of this slice
    if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == GetSliceHeader()->disable_deblocking_filter_idc || m_bPrevDeblocked)
    {
        if (false == IsSliceGroups())
            return true;
    }

    return false;
}

void H264SliceEx::CompleteDecoding()
{
    //if (m_bDecoded)//  && m_bDeblocked) - we do not need to wait deblocking because FreeResources frees coeff buffer
        FreeResources();
}

bool H264SliceEx::Reset(H264NalExtension *pNalExt)
{
    if (!H264Slice::Reset(pNalExt))
        return false;

    m_iCurMBToDec = m_iFirstMB;
    m_iCurMBToRec = m_iFirstMB;
    m_iCurMBToDeb = m_iFirstMB;

    m_bInProcess = false;
    m_bDecVacant = 1;
    m_bRecVacant = 1;
    m_bDebVacant = 1;
    m_MVsDistortion = 0;

    // reset through-decoding variables
    m_nMBSkipCount = 0;
    m_nQuantPrev = m_pPicParamSet->pic_init_qp +
                   m_SliceHeader.slice_qp_delta;
    m_prev_dquant = 0;
    m_field_index = m_SliceHeader.field_pic_flag && m_SliceHeader.bottom_field_flag ? 1 : 0;

    if (IsSliceGroups())
        m_bNeedToCheckMBSliceEdges = true;
    else
        m_bNeedToCheckMBSliceEdges = m_SliceHeader.first_mb_in_slice != 0;

    if (m_bDeblocked)
    {
        m_bDebVacant = 0;
        m_iCurMBToDeb = m_iMaxMB;
    }

    return true;
}

Status H264SliceEx::UpdateReferenceList(ViewList &views, Ipp32s dIdIndex)
{
    Status sts = H264Slice::UpdateReferenceList(views, dIdIndex);
    if (sts == UMC_OK)
    {
        H264DecoderFrame **pRefPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_RefPicList;
        H264DecoderFrame **pRefPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_RefPicList;
        ReferenceFlags *pFields0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_Flags;
        ReferenceFlags *pFields1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_Flags;

        // If B slice, init scaling factors array
        if ((BPREDSLICE == m_SliceHeader.slice_type) && (pRefPicList1[0] != NULL))
        {
            InitDistScaleFactor(m_SliceHeader.num_ref_idx_l0_active,
                                m_SliceHeader.num_ref_idx_l1_active,
                                pRefPicList0, pRefPicList1, pFields0, pFields1);
        }
    }

    return sts;
}

#define CalculateDSF(index, value, value_mv)                                                     \
        /* compute scaling ratio for temporal direct and implicit weighting*/   \
        tb = picCntCur - picCntRef0;    /* distance from previous */            \
        td = picCntRef1 - picCntRef0;    /* distance between ref0 and ref1 */   \
                                                                                \
        /* special rule: if td is 0 or if L0 is long-term reference, use */     \
        /* L0 motion vectors and equal weighting.*/                             \
        if (td == 0 || pRefPicList0[index]->isLongTermRef(pRefPicList0[index]->GetNumberByParity(RefFieldTop)))  \
        {                                                                       \
            /* These values can be used directly in scaling calculations */     \
            /* to get back L0 or can use conditional test to choose L0.    */   \
            value = 128;    /* for equal weighting    */    \
            value_mv = 256;                                  \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            tb = IPP_MAX(-128,tb);                                              \
            tb = IPP_MIN(127,tb);                                               \
            td = IPP_MAX(-128,td);                                              \
            td = IPP_MIN(127,td);                                               \
                                                                                \
            VM_ASSERT(td != 0);                                                 \
                                                                                \
            tx = (16384 + abs(td/2))/td;                                        \
                                                                                \
            DistScaleFactor = (tb*tx + 32)>>6;                                  \
            DistScaleFactor = IPP_MAX(-1024, DistScaleFactor);                  \
            DistScaleFactor = IPP_MIN(1023, DistScaleFactor);                   \
                                                                                \
            if (isL1LongTerm || DistScaleFactor < -256 || DistScaleFactor > 512)                \
                value = 128;    /* equal weighting     */   \
            else                                                                \
                value = (FactorArrayValue)DistScaleFactor;                    \
                                                                                \
            value_mv = (FactorArrayValue)DistScaleFactor;                      \
        }

void H264SliceEx::InitDistScaleFactor(Ipp32s NumL0RefActive,
                                    Ipp32s NumL1RefActive,
                                    H264DecoderFrame **pRefPicList0,
                                    H264DecoderFrame **pRefPicList1,
                                    ReferenceFlags *pFields0,
                                    ReferenceFlags *pFields1)

{
    Ipp32s L0Index, L1Index;
    Ipp32s picCntRef0;
    Ipp32s picCntRef1;
    Ipp32s picCntCur;
    Ipp32s DistScaleFactor;
    FactorArrayValue *pDistScaleFactor;
    FactorArrayValue *pDistScaleFactorMV;

    Ipp32s tb;
    Ipp32s td;
    Ipp32s tx;

    VM_ASSERT(NumL0RefActive <= MAX_NUM_REF_FRAMES);
    VM_ASSERT(pRefPicList1[0]);

    bool isL1LongTerm = false;

    picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(m_SliceHeader.bottom_field_flag));

    bool isNeedScale = (m_pPicParamSet->weighted_bipred_idc == 2);

    if (!isNeedScale)
        NumL1RefActive = 1;

    for (L1Index = NumL1RefActive - 1; L1Index >=0 ; L1Index--)
    {
        pDistScaleFactor = m_DistScaleFactor.values[L1Index];        //frames or fields
        pDistScaleFactorMV = m_DistScaleFactorMV.values;  //frames or fields

        if (!pRefPicList1[L1Index])
            continue;

        Ipp32s RefField = m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE ?
            0 : GetReferenceField(pFields1, L1Index);

        picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(RefField));
        isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(RefField));

        for (L0Index = 0; L0Index < NumL0RefActive; L0Index++)
        {
            if (!pRefPicList0[L0Index])
                continue;

            Ipp32s RefFieldTop = (m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE) ?
                0 : GetReferenceField(pFields0, L0Index);
            picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(RefFieldTop));

            CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
        }
    }

    if (m_pCurrentFrame->m_PictureStructureForDec == AFRM_STRUCTURE)
    {
        if (!m_DistScaleFactorAFF && isNeedScale)
            m_DistScaleFactorAFF = (FactorArrayAFF*)m_pObjHeap->Allocate(sizeof(FactorArrayAFF));

        if (!m_DistScaleFactorMVAFF)
            m_DistScaleFactorMVAFF = (FactorArrayMVAFF*)m_pObjHeap->Allocate(sizeof(FactorArrayMVAFF));

        if (!isNeedScale)
        {
            m_DistScaleFactorAFF = &m_StaticFactorArrayAFF;
        }

        for (L1Index = NumL1RefActive - 1; L1Index >=0 ; L1Index--)
        {
            // [curmb field],[ref1field],[ref0field]
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][0][0];      //complementary field pairs, cf=top r1=top,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][0][0];  //complementary field pairs, cf=top r1=top,r0=top

            if (!pRefPicList1[L1Index])
                continue;

            picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(0), 1);
            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(0), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(0));

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][1][0];        //complementary field pairs, cf=top r1=top,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][1][0];  //complementary field pairs, cf=top r1=top,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 1;

                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][0][1];        //complementary field pairs, cf=top r1=top,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][0][1];  //complementary field pairs, cf=top r1=top,r0=top

            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(1), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(1));

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][1][1];        //complementary field pairs, cf=top r1=top,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][1][1];  //complementary field pairs, cf=top r1=top,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 1;

                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1),1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            /*--------------------------------------------------------------------*/
            picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(1), 1);
            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(0), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(0));

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][0][0];        //complementary field pairs, cf=bottom r1=bottom,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][0][0];  //complementary field pairs, cf=bottom r1=bottom,r0=top

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][1][0];       //complementary field pairs, cf=bottom r1=bottom,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][1][0];   //complementary field pairs, cf=bottom r1=bottom,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 1;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(1), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(1));

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][0][1];        //complementary field pairs, cf=bottom r1=bottom,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][0][1];  //complementary field pairs, cf=bottom r1=bottom,r0=top

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][1][1];       //complementary field pairs, cf=bottom r1=bottom,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][1][1];   //complementary field pairs, cf=bottom r1=bottom,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                if (!pRefPicList0[L0Index])
                    continue;

                Ipp32s RefFieldTop = 1;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
        }

        if (!isNeedScale)
        {
            m_DistScaleFactorAFF = 0;
        }
    }
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
