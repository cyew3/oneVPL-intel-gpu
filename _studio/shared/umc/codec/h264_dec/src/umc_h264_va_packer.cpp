// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_va_packer.h"
#include "umc_h264_task_supplier.h"

#ifdef UMC_VA_DXVA
#include "umc_mvc_ddi.h"
#include "umc_svc_ddi.h"
#include "umc_va_dxva2_protected.h"
#endif
#ifdef UMC_VA_LINUX
#include "umc_va_linux.h"
#include "umc_va_linux_protected.h"
#include "umc_va_video_processing.h"

#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"
#endif

#include "mfx_trace.h"

namespace UMC
{

enum ChoppingStatus
{
    CHOPPING_NONE = 0,
    CHOPPING_SPLIT_SLICE_DATA = 1,
    CHOPPING_SPLIT_SLICES = 2,
    CHOPPING_SKIP_SLICE,
};

Packer::Packer(VideoAccelerator * va, TaskSupplier * supplier)
    : m_va(va)
    , m_supplier(supplier)
{

}

Packer::~Packer()
{
}

Status Packer::SyncTask(H264DecoderFrame* pFrame, void * error)
{
    return m_va->SyncTask(pFrame->m_index, error);
}

Status Packer::QueryTaskStatus(int32_t index, void * status, void * error)
{
    return m_va->QueryTaskStatus(index, status, error);
}

Status Packer::QueryStreamOut(H264DecoderFrame* pFrame)
{
    (void)pFrame;

    return UMC_OK;
}

Packer * Packer::CreatePacker(VideoAccelerator * va, TaskSupplier* supplier)
{
    (void)va;
    (void)supplier;

    Packer * packer = 0;
#if defined(UMC_VA_DXVA)
        packer = new PackerDXVA2(va, supplier);
#else
#if defined (UMC_VA_LINUX)
        packer = new PackerVA(va, supplier);
#endif // UMC_VA_LINUX
#endif

    return packer;
}

#ifdef UMC_VA_DXVA
enum
{
    NO_REFERENCE = 0,
    SHORT_TERM_REFERENCE = 1,
    LONG_TERM_REFERENCE = 2,
    INTERVIEW_TERM_REFERENCE = 3
};

/****************************************************************************************************/
// DXVA Windows packer implementation
/****************************************************************************************************/
PackerDXVA2::PackerDXVA2(VideoAccelerator * va, TaskSupplier * supplier)
    : Packer(va, supplier)
    , m_pBuf(NULL)
    , m_statusReportFeedbackCounter(1)
    , m_picParams(0)
{
}

Status PackerDXVA2::SyncTask(H264DecoderFrame* pFrame, void * error)
{
    return m_va->SyncTask(pFrame->GetFrameData()->GetFrameMID(), error);
}

Status PackerDXVA2::GetStatusReport(void * pStatusReport, size_t size)
{
    return m_va->ExecuteStatusReportBuffer(pStatusReport, (uint32_t)size);
}

void PackerDXVA2::PackAU(H264DecoderFrameInfo * sliceInfo, int32_t first_slice, int32_t count_all)
{
    if (!m_va || !count_all)
        return;

    uint32_t sliceStructSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Slice_H264_Long) : sizeof(DXVA_Slice_H264_Short);
    int32_t numSlicesOfPrevField = first_slice;

    first_slice = 0;
    H264Slice* slice = sliceInfo->GetSlice(first_slice);

    const UMC_H264_DECODER::H264ScalingPicParams * scaling = &slice->GetPicParam()->scaling[NAL_UT_CODED_SLICE_EXTENSION == slice->GetSliceHeader()->nal_unit_type ? 1 : 0];
    PackQmatrix(scaling);

    int32_t chopping = CHOPPING_NONE;

    for ( ; count_all; )
    {
        PackPicParams(sliceInfo, slice);

        uint32_t partial_count = count_all;

        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &CompBuf);

        if (CompBuf->GetBufferSize() / sliceStructSize < partial_count)
            partial_count = CompBuf->GetBufferSize() / sliceStructSize;

        for (uint32_t i = 0; i < partial_count; i++)
        {
            // put slice header
            H264Slice *pSlice = sliceInfo->GetSlice(first_slice + i);
            chopping = PackSliceParams(pSlice, i, chopping, numSlicesOfPrevField);
            if (chopping != CHOPPING_NONE)
            {
                partial_count = i;
                break;
            }
        }

        uint32_t passedSliceNum = chopping == CHOPPING_SPLIT_SLICE_DATA ? partial_count + 1 : partial_count;
        CompBuf->FirstMb = 0;
        CompBuf->NumOfMB = passedSliceNum;
        CompBuf->SetDataSize(passedSliceNum * sliceStructSize);

        uint8_t *pDXVA_BitStreamBuffer = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        int32_t AlignedNalUnitSize = mfx::align2_value(CompBuf->GetDataSize(), 128);
        pDXVA_BitStreamBuffer += CompBuf->GetDataSize();
        memset(pDXVA_BitStreamBuffer, 0, AlignedNalUnitSize - CompBuf->GetDataSize());
        CompBuf->SetDataSize(AlignedNalUnitSize);
        CompBuf->NumOfMB = passedSliceNum;

        Status sts = m_va->Execute();
        if (sts != UMC_OK)
        {
            throw h264_exception(sts);
        }

        first_slice += partial_count;
        count_all -= partial_count;
    }

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    if (m_va->GetProtectedVA())
    {
        mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

        if (bs && bs->EncryptedData)
        {
            int32_t count = sliceInfo->GetSliceCount();
            m_va->GetProtectedVA()->MoveBSCurrentEncrypt(count);
        }
    }
#endif
}

void PackerDXVA2::PackAU(const H264DecoderFrame *pFrame, int32_t field)
{
    H264DecoderFrameInfo * sliceInfo = const_cast<H264DecoderFrameInfo *>(pFrame->GetAU(field));
    int32_t count_all = sliceInfo->GetSliceCount();

    if (!count_all)
        return;

    int32_t first_slice = field ? pFrame->GetAU(0)->GetSliceCount() : 0;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    if (m_va && m_va->GetProtectedVA())
    {
        if (sliceInfo->m_pFrame->m_viewId == m_supplier->GetBaseViewId() && !field)
        {
            first_slice = 0;
            m_va->GetProtectedVA()->ResetBSCurrentEncrypt();
        }
        else
        {
            first_slice = m_va->GetProtectedVA()->GetBSCurrentEncrypt();
        }
    }
#endif

    PackAU(sliceInfo, first_slice, count_all);
}

void PackerDXVA2::AddReferenceFrame(DXVA_PicParams_H264 * pPicParams_H264, int32_t &pos,
                       H264DecoderFrame * pFrame, int32_t reference)
{
    pPicParams_H264->RefFrameList[pos].Index7Bits = GetFrameIndex(pFrame).Index7Bits;
    pPicParams_H264->RefFrameList[pos].AssociatedFlag = reference > SHORT_TERM_REFERENCE ? 1 : 0;

    pPicParams_H264->FieldOrderCntList[pos][0] = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(0)];
    pPicParams_H264->FieldOrderCntList[pos][1] = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(1)];

    if (reference)
    {
        pPicParams_H264->FrameNumList[pos] = (reference == LONG_TERM_REFERENCE) ?
            (USHORT)pFrame->m_LongTermFrameIdx : (USHORT)pFrame->m_FrameNum;

        if (pFrame->isShortTermRef(pFrame->GetNumberByParity(0)) || pFrame->isLongTermRef(pFrame->GetNumberByParity(0)))
            pPicParams_H264->UsedForReferenceFlags |= 1<<(2*pos);

        if (pFrame->isShortTermRef(pFrame->GetNumberByParity(1)) || pFrame->isLongTermRef(pFrame->GetNumberByParity(1)))
            pPicParams_H264->UsedForReferenceFlags |= 1<<(2*pos + 1);

        if (!pFrame->isShortTermRef() && !pFrame->isLongTermRef() && (pFrame->isInterViewRef(0) || pFrame->isInterViewRef(1)))
        {
            pPicParams_H264->UsedForReferenceFlags |= 1<<(2*pos);
            pPicParams_H264->UsedForReferenceFlags |= 1<<(2*pos + 1);
        }

        if (!pFrame->IsFrameExist())
        {
            pPicParams_H264->NonExistingFrameFlags |= 1<<pos;
            pPicParams_H264->RefFrameList[pos].Index7Bits = 0x7F;
            pPicParams_H264->RefFrameList[pos].AssociatedFlag = 1;
        }
    }
    else
    {
        pPicParams_H264->FrameNumList[pos] = 0;
        pPicParams_H264->RefFrameList[pos].Index7Bits = 0x7F;
        pPicParams_H264->FieldOrderCntList[pos][0] = 0;
        pPicParams_H264->FieldOrderCntList[pos][1] = 0;
        pPicParams_H264->RefFrameList[pos].AssociatedFlag = 1;
    }
}

void PackerDXVA2::PackSliceGroups(DXVA_PicParams_H264 * pPicParams_H264, H264DecoderFrame * frame)
{
    UCHAR *groupMap = pPicParams_H264->SliceGroupMap;

    H264Slice * slice = frame->GetAU(0)->GetSlice(0);
    UMC_H264_DECODER::H264PicParamSet *pps = const_cast<UMC_H264_DECODER::H264PicParamSet *>(slice->GetPicParam());
    UMC_H264_DECODER::H264SliceHeader * sliceHeader = slice->GetSliceHeader();

    uint32_t uNumSliceGroups = pps->num_slice_groups;
    uint32_t uNumMBCols = slice->GetSeqParam()->frame_width_in_mbs;
    uint32_t uNumMBRows = slice->GetSeqParam()->frame_height_in_mbs;
    uint32_t uNumMapUnits = uNumMBCols*uNumMBRows;

    UCHAR mapUnitToSliceGroupMap[810];

    for(uint32_t i = 0; i < 810; i++)
        mapUnitToSliceGroupMap[i] = 0;

    if (!(uNumSliceGroups > 1))
        return;

    switch (pps->SliceGroupInfo.slice_group_map_type)
    {
        case 0:
        {
            // interleaved slice groups: run_length for each slice group,
            // repeated until all MB's are assigned to a slice group

            uint32_t i = 0;
            do
                for(uint32_t iGroup = 0; iGroup < uNumSliceGroups && i < uNumMapUnits; i += pps->SliceGroupInfo.run_length[iGroup++])
                    for(uint32_t j = 0; j < pps->SliceGroupInfo.run_length[iGroup] && i + j < uNumMapUnits; j++)
                        mapUnitToSliceGroupMap[i + j] = (UCHAR) iGroup;
            while(i < uNumMapUnits);
        }
        break;

    case 1:
        {
            // dispersed
            for(uint32_t i = 0; i < uNumMapUnits; i++ )
                mapUnitToSliceGroupMap[i] = (UCHAR) (((i % uNumMBCols) + (((i / uNumMBRows) * uNumSliceGroups) / 2)) % uNumSliceGroups);
        }
        break;

    case 2:
        {
            // foreground + leftover: Slice groups are rectangles, any MB not
            // in a defined rectangle is in the leftover slice group, a MB within
            // more than one rectangle is in the lower-numbered slice group.

            for(uint32_t i = 0; i < uNumMapUnits; i++)
                mapUnitToSliceGroupMap[i] = (UCHAR) (uNumSliceGroups - 1);

            for(int32_t iGroup = uNumSliceGroups - 2; iGroup >= 0; iGroup--)
            {
                uint32_t yTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] / uNumMBCols;
                uint32_t xTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] % uNumMBCols;
                uint32_t yBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] / uNumMBCols;
                uint32_t xBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] % uNumMBCols;

                for(uint32_t y = yTopLeft; y <= yBottomRight; y++)
                    for(uint32_t x = xTopLeft; x <= xBottomRight; x++)
                        mapUnitToSliceGroupMap[y * uNumMBCols + x] = (UCHAR) iGroup;
            }
        }
        break;

    case 3:
        {
            // Box-out, clockwise or counterclockwise. Result is two slice groups,
            // group 0 included by the box, group 1 excluded.

            uint32_t x, y, leftBound, rightBound, topBound, bottomBound;
            uint32_t mapUnitVacant = 0;
            int8_t xDir, yDir;
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;

            x = leftBound = rightBound = (uNumMBCols - dir_flag) / 2;
            y = topBound = bottomBound = (uNumMBRows - dir_flag) / 2;

            xDir  = dir_flag - 1;
            yDir  = dir_flag;

            uint32_t uNumInGroup0 = std::min(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, uNumMapUnits);

            for(uint32_t i = 0; i < uNumMapUnits; i++)
                mapUnitToSliceGroupMap[i] = 1;

            for(uint32_t k = 0; k < uNumInGroup0; k += mapUnitVacant)
            {
                mapUnitVacant = (mapUnitToSliceGroupMap[y * uNumMBCols + x] == 1);

                if(mapUnitVacant)
                    mapUnitToSliceGroupMap[y * uNumMBCols + x] = 0;

                if(xDir == -1 && x == leftBound)
                {
                    leftBound = std::max(leftBound - 1, 0u);
                    x = leftBound;
                    xDir = 0;
                    yDir = 2 * dir_flag - 1;
                }
                else if(xDir == 1 && x == rightBound)
                {
                    rightBound = std::min(rightBound + 1, uNumMBCols - 1);
                    x = rightBound;
                    xDir = 0;
                    yDir = 1 - 2 * dir_flag;
                }
                else if(yDir == -1 && y == topBound)
                {
                    topBound = std::max(topBound - 1, 0u);
                    y = topBound;
                    xDir = 1 - 2 * dir_flag;
                    yDir = 0;
                }
                else if(yDir == 1 && y == bottomBound)
                {
                    bottomBound = std::min(bottomBound + 1, uNumMBRows - 1);
                    y = bottomBound;
                    xDir = 2 * dir_flag - 1;
                    yDir = 0;
                }
                else
                {
                    x += xDir;
                    y += yDir;
                }
            }
        }
        break;

    case 4:
        {
            // raster-scan: 2 slice groups

            uint32_t uNumInGroup0 = std::min(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, uNumMapUnits);
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            uint32_t sizeOfUpperLeftGroup = (dir_flag ? (uNumMapUnits - uNumInGroup0) : uNumInGroup0);

            for(uint32_t i = 0; i < uNumMapUnits; i++)
                if(i < sizeOfUpperLeftGroup)
                    mapUnitToSliceGroupMap[i] = dir_flag;
                else
                    mapUnitToSliceGroupMap[i] = 1 - dir_flag;
        }
        break;

    case 5:
        {
            // wipe: 2 slice groups, the vertical version of case 4.
            //  L L L L R R R R R
            //  L L L L R R R R R
            //  L L L R R R R R R
            //  L L L R R R R R R

            uint32_t uNumInGroup0 = std::min(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, uNumMapUnits);
            uint8_t dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            uint32_t sizeOfUpperLeftGroup = (dir_flag ? (uNumMapUnits - uNumInGroup0) : uNumInGroup0);

            uint32_t k = 0;
            for(uint32_t j = 0; j < uNumMBCols; j++)
                for(uint32_t i = 0; i < uNumMBRows; i++)
                    if(k++ < sizeOfUpperLeftGroup)
                        mapUnitToSliceGroupMap[i * uNumMBCols + j] = dir_flag;
                    else
                        mapUnitToSliceGroupMap[i * uNumMBCols + j] = 1 - dir_flag;
        }
        break;

    case 6:
        {
            // explicit map read from bitstream, contains slice group id for
            // each map unit
            for(uint32_t i = 0; i < pps->SliceGroupInfo.pSliceGroupIDMap.size(); i++)
                mapUnitToSliceGroupMap[i] = pps->SliceGroupInfo.pSliceGroupIDMap[i];
        }
        break;

    default:
        // can't happen
        VM_ASSERT(0);
    }    // switch map type

    // Filling array groupMap as in H264 standard
    //if(pPicParams_H264->frame_mbs_only_flag == 1 || pPicParams_H264->field_pic_flag == 1)
    //{
    //  for(uint32_t i = 0; i < uNumMapUnits; i++ )
    //  {
    //      groupMap[i] = mapUnitToSliceGroupMap[i];
    //  }
    //}
    //else if(pPicParams_H264->MbaffFrameFlag == 1)
    //{
    //  for(uint32_t i = 0; i < uNumMapUnits; i++ )
    //  {
    //      groupMap[i] = mapUnitToSliceGroupMap[i/2];
    //  }
    //}
    //else if(pPicParams_H264->frame_mbs_only_flag == 0 && pPicParams_H264->MbaffFrameFlag == 0 && pPicParams_H264->field_pic_flag == 0)
    //{
    //  for(uint32_t i = 0; i < uNumMapUnits; i++ )
    //  {
    //      groupMap[i] = mapUnitToSliceGroupMap[(i / (2 * uNumMBCols)) * uNumMBCols + ( i % uNumMBCols )];
    //  }
    //}

    // Filling array groupMap as in MS DXVA for H264
    for(uint32_t i = 0; i < uNumMapUnits; i++ )
        groupMap[i>>1] = (UCHAR) (groupMap[i >> 1] + (mapUnitToSliceGroupMap[i] << (4 * (i & 1))));
}

void PackerDXVA2::PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_Intel_PicParams_MVC* pMVCPicParams_H264)
{
    const H264Slice  * pSlice = pSliceInfo->GetSlice(0);
    const UMC_H264_DECODER::H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    memset(pMVCPicParams_H264, 0, sizeof(DXVA_Intel_PicParams_MVC));

    pMVCPicParams_H264->CurrViewID = pSliceHeader->nal_ext.mvc.view_id;
    pMVCPicParams_H264->anchor_pic_flag = pSliceHeader->nal_ext.mvc.anchor_pic_flag;
    pMVCPicParams_H264->inter_view_flag = pSliceHeader->nal_ext.mvc.inter_view_flag;

    pMVCPicParams_H264->SwitchToAVC = 0;

    if (pSliceHeader->nal_ext.mvc.view_id)
    {
        uint32_t VOIdx = GetVOIdx(pSlice->GetSeqMVCParam(), pSliceHeader->nal_ext.mvc.view_id);
        const UMC_H264_DECODER::H264ViewRefInfo &refInfo = pSlice->GetSeqMVCParam()->viewInfo[VOIdx];

        pMVCPicParams_H264->NumInterViewRefsL0 = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.num_anchor_refs_lx[0] : refInfo.num_non_anchor_refs_lx[0];
        pMVCPicParams_H264->NumInterViewRefsL1 = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.num_anchor_refs_lx[1] : refInfo.num_non_anchor_refs_lx[1];

        for (uint32_t i = 0; i < sizeof(pMVCPicParams_H264->InterViewRefList[0])/sizeof(pMVCPicParams_H264->InterViewRefList[0][0]); i++)
        {
            if (i < pMVCPicParams_H264->NumInterViewRefsL0)
                pMVCPicParams_H264->InterViewRefList[0][i] = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.anchor_refs_lx[0][i] : refInfo.non_anchor_refs_lx[0][i];
            else
                pMVCPicParams_H264->InterViewRefList[0][i] = (USHORT)0xFFFF;

            if (i < pMVCPicParams_H264->NumInterViewRefsL1)
                pMVCPicParams_H264->InterViewRefList[1][i] = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.anchor_refs_lx[1][i] : refInfo.non_anchor_refs_lx[1][i];
            else
                pMVCPicParams_H264->InterViewRefList[1][i] = (USHORT)0xFFFF;
        }
    }
    else
    {
        pMVCPicParams_H264->NumInterViewRefsL0 = 0;
        pMVCPicParams_H264->NumInterViewRefsL1 = 0;

        for (uint32_t i = 0; i < sizeof(pMVCPicParams_H264->InterViewRefList[0])/sizeof(pMVCPicParams_H264->InterViewRefList[0][0]); i++)
        {
            pMVCPicParams_H264->InterViewRefList[0][i] = (USHORT)0xFFFF;
            pMVCPicParams_H264->InterViewRefList[1][i] = (USHORT)0xFFFF;
        }
    }
}

void PackerDXVA2::PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_PicParams_H264_MVC* pMVCPicParams_H264)
{
    const H264Slice  * pSlice = pSliceInfo->GetSlice(0);
    const UMC_H264_DECODER::H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    memset(pMVCPicParams_H264, 0, sizeof(DXVA_PicParams_H264_MVC));

    pMVCPicParams_H264->curr_view_id = pSliceHeader->nal_ext.mvc.view_id;
    pMVCPicParams_H264->anchor_pic_flag = pSliceHeader->nal_ext.mvc.anchor_pic_flag;
    pMVCPicParams_H264->inter_view_flag = pSliceHeader->nal_ext.mvc.inter_view_flag;

    pMVCPicParams_H264->num_views_minus1 = (UCHAR)pSlice->GetSeqMVCParam()->num_views_minus1;

    for (int32_t view_id = 0; view_id < sizeof(pMVCPicParams_H264->view_id)/sizeof(pMVCPicParams_H264->view_id[0]); view_id++)
    {
        pMVCPicParams_H264->view_id[view_id] = (USHORT)0xFFFF;
    }

    for (int32_t view_id = 0; view_id <= pMVCPicParams_H264->num_views_minus1; view_id++)
    {
        const UMC_H264_DECODER::H264ViewRefInfo & refInfo = pSlice->GetSeqMVCParam()->viewInfo[view_id];
        pMVCPicParams_H264->view_id[view_id] = (USHORT)refInfo.view_id;

        pMVCPicParams_H264->num_anchor_refs_l0[view_id] = refInfo.num_anchor_refs_lx[0];
        for (uint32_t i = 0; i < 16; i++)
            pMVCPicParams_H264->anchor_ref_l0[view_id][i] = refInfo.anchor_refs_lx[0][i];

        pMVCPicParams_H264->num_anchor_refs_l1[view_id] = refInfo.num_anchor_refs_lx[1];
        for (uint32_t i = 0; i < 16; i++)
            pMVCPicParams_H264->anchor_ref_l1[view_id][i] = refInfo.anchor_refs_lx[1][i];

        pMVCPicParams_H264->num_non_anchor_refs_l0[view_id] = refInfo.num_non_anchor_refs_lx[0];
        for (uint32_t i = 0; i < 16; i++)
            pMVCPicParams_H264->non_anchor_ref_l0[view_id][i] = refInfo.non_anchor_refs_lx[0][i];

        pMVCPicParams_H264->num_non_anchor_refs_l1[view_id] = refInfo.num_non_anchor_refs_lx[1];
        for (uint32_t i = 0; i < 16; i++)
            pMVCPicParams_H264->non_anchor_ref_l1[view_id][i] = refInfo.non_anchor_refs_lx[1][i];
    }
}

void PackerDXVA2::PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice)
{
    UMCVACompBuffer *picParamBuf;
    DXVA_PicParams_H264* pPicParams_H264 = (DXVA_PicParams_H264*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &picParamBuf);
    memset(pPicParams_H264, 0, sizeof(DXVA_PicParams_H264));

    PackPicParams(pSliceInfo, pSlice, pPicParams_H264);

    picParamBuf->SetDataSize(sizeof(DXVA_PicParams_H264));

    if (m_va->IsMVCSupport())
    {
        if ((m_va->m_Profile & VA_PROFILE) == VA_PROFILE_MVC) // intel MVC profile
        {
        }
        else
        {
            picParamBuf->SetDataSize(sizeof(DXVA_PicParams_H264_MVC));
        }
    }
}

void PackerDXVA2::PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice, DXVA_PicParams_H264* pPicParams_H264)
{
    const UMC_H264_DECODER::H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();
    const UMC_H264_DECODER::H264SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const UMC_H264_DECODER::H264PicParamSet *pPicParamSet = pSlice->GetPicParam();

    const H264DecoderFrame *pCurrentFrame = pSliceInfo->m_pFrame;

    m_picParams = pPicParams_H264;

    DXVA_Intel_PicParams_MVC* picParamsIntelMVC = 0;
    DXVA_PicParams_H264_MVC * picParamsMSMVC = 0;
    if (m_va->IsMVCSupport())
    {
        if ((m_va->m_Profile & VA_PROFILE) == VA_PROFILE_MVC) // intel MVC profile
        {
            UMCVACompBuffer *picMVCParamBuf;
            picParamsIntelMVC = (DXVA_Intel_PicParams_MVC*)m_va->GetCompBuffer(DXVA_MVCPictureParametersExtBufferType, &picMVCParamBuf);

            if (!picParamsIntelMVC)
                throw h264_exception(UMC_ERR_FAILED);

            memset(picParamsIntelMVC, 0, sizeof(DXVA_Intel_PicParams_MVC));
            PackPicParamsMVC(pSliceInfo, picParamsIntelMVC);
            picMVCParamBuf->SetDataSize(sizeof(DXVA_Intel_PicParams_MVC));
        }
        else
        {
            picParamsMSMVC = (DXVA_PicParams_H264_MVC*)pPicParams_H264;

            if (!picParamsMSMVC)
                throw h264_exception(UMC_ERR_FAILED);

            memset(picParamsMSMVC, 0, sizeof(DXVA_PicParams_H264_MVC));
            PackPicParamsMVC(pSliceInfo, picParamsMSMVC);
        }
    }

    //packing
    pPicParams_H264->wFrameWidthInMbsMinus1 = (USHORT)(pSeqParamSet->frame_width_in_mbs - 1);
    pPicParams_H264->wFrameHeightInMbsMinus1 = (USHORT)(pSeqParamSet->frame_height_in_mbs - 1);
    pPicParams_H264->CurrPic.Index7Bits = (UCHAR)pCurrentFrame->m_index;
    pPicParams_H264->CurrPic.AssociatedFlag = pSliceHeader->bottom_field_flag;
    pPicParams_H264->num_ref_frames = (UCHAR)pSeqParamSet->num_ref_frames;
    pPicParams_H264->field_pic_flag = pSliceHeader->field_pic_flag;
    pPicParams_H264->MbaffFrameFlag = pSliceHeader->MbaffFrameFlag;
    pPicParams_H264->residual_colour_transform_flag = pSeqParamSet->residual_colour_transform_flag;
    pPicParams_H264->sp_for_switch_flag  = pSliceHeader->sp_for_switch_flag;
    pPicParams_H264->chroma_format_idc = pSeqParamSet->chroma_format_idc;
    pPicParams_H264->RefPicFlag = (USHORT)((pSliceHeader->nal_ref_idc == 0) ? 0 : 1);
    pPicParams_H264->constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
    pPicParams_H264->weighted_pred_flag = pPicParamSet->weighted_pred_flag;
    pPicParams_H264->weighted_bipred_idc = pPicParamSet->weighted_bipred_idc;
    pPicParams_H264->MbsConsecutiveFlag = (USHORT)(pPicParamSet->num_slice_groups > 1 ? 0 : 1);
    pPicParams_H264->frame_mbs_only_flag = pSeqParamSet->frame_mbs_only_flag;
    pPicParams_H264->transform_8x8_mode_flag = pPicParamSet->transform_8x8_mode_flag;
    pPicParams_H264->MinLumaBipredSize8x8Flag = pSeqParamSet->level_idc > 30 ? 1 : 0;

    pPicParams_H264->IntraPicFlag = pSliceInfo->IsIntraAU(); //1 ???

    pPicParams_H264->bit_depth_luma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
    pPicParams_H264->bit_depth_chroma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);

    pPicParams_H264->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;

    //create reference picture list
    for (int32_t i = 0; i < 16; i++)
    {
        pPicParams_H264->RefFrameList[i].bPicEntry = 0xff;
    }

    //pack POC
    if (pPicParams_H264->field_pic_flag == 1)
    {
        pPicParams_H264->CurrFieldOrderCnt[pSliceHeader->bottom_field_flag] =
            pCurrentFrame->m_PicOrderCnt[pCurrentFrame->GetNumberByParity(pSliceHeader->bottom_field_flag)];
    }
    else
    {
        pPicParams_H264->CurrFieldOrderCnt[0] = pCurrentFrame->m_PicOrderCnt[pCurrentFrame->GetNumberByParity(0)];
        pPicParams_H264->CurrFieldOrderCnt[1] = pCurrentFrame->m_PicOrderCnt[pCurrentFrame->GetNumberByParity(1)];
    }

    pPicParams_H264->pic_init_qs_minus26 = (CHAR)(pPicParamSet->pic_init_qs - 26);
    pPicParams_H264->chroma_qp_index_offset = pPicParamSet->chroma_qp_index_offset[0];
    pPicParams_H264->second_chroma_qp_index_offset = pPicParamSet->chroma_qp_index_offset[1];

    //1 VLD
    if (!(m_va->m_Profile & VA_VLD))
        return;

    pPicParams_H264->ContinuationFlag = 1;
    pPicParams_H264->pic_init_qp_minus26 = (CHAR)(pPicParamSet->pic_init_qp - 26);
    pPicParams_H264->num_ref_idx_l0_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l0_active-1);
    pPicParams_H264->num_ref_idx_l1_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l1_active-1);

    pPicParams_H264->NonExistingFrameFlags = 0;
    pPicParams_H264->frame_num = (USHORT)pSliceHeader->frame_num;
    pPicParams_H264->log2_max_frame_num_minus4 = (UCHAR)(pSeqParamSet->log2_max_frame_num - 4);
    pPicParams_H264->pic_order_cnt_type = pSeqParamSet->pic_order_cnt_type;
    pPicParams_H264->log2_max_pic_order_cnt_lsb_minus4 = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParams_H264->delta_pic_order_always_zero_flag = pSeqParamSet->delta_pic_order_always_zero_flag;
    pPicParams_H264->direct_8x8_inference_flag = pSeqParamSet->direct_8x8_inference_flag;
    pPicParams_H264->entropy_coding_mode_flag = pPicParamSet->entropy_coding_mode;
    pPicParams_H264->pic_order_present_flag = pPicParamSet->bottom_field_pic_order_in_frame_present_flag;
    // pPicParams_H264->num_slice_groups_minus1 = (UCHAR)(pPicParamSet->num_slice_groups - 1);
    // pPicParams_H264->slice_group_map_type = pPicParamSet->SliceGroupInfo.slice_group_map_type;
    pPicParams_H264->deblocking_filter_control_present_flag = pPicParamSet->deblocking_filter_variables_present_flag;
    pPicParams_H264->redundant_pic_cnt_present_flag = 0;//pPicParamSet->redundant_pic_cnt_present_flag;
    pPicParams_H264->slice_group_change_rate_minus1 = (USHORT)(pPicParamSet->SliceGroupInfo.t2.slice_group_change_rate ?
        pPicParamSet->SliceGroupInfo.t2.slice_group_change_rate - 1 : 0);

    if (pCurrentFrame->isInterViewRef(pPicParams_H264->field_pic_flag ? pCurrentFrame->GetNumberByParity(pSliceHeader->bottom_field_flag) : 0) && !pPicParams_H264->RefPicFlag)
        pPicParams_H264->RefPicFlag = 1;

    PackSliceGroups(pPicParams_H264, const_cast<H264DecoderFrame *>(pCurrentFrame));

    int32_t j = 0;

    bool currViewVisited = false;

    int32_t viewCount = m_supplier->GetViewCount();
    for (int32_t i = 0; i < viewCount; i++)
    {
        ViewItem & view = m_supplier->GetViewByNumber(i);
        H264DBPList * pDPBList = view.GetDPBList(0);
        int32_t dpbSize = pDPBList->GetDPBSize();

        int32_t start = j;

        /*if (m_va->IsMVCSupport() && view.viewId != pSliceHeader->nal_ext.mvc.view_id && picParamsMSMVC)
        {
            continue;
        }*/

        if (view.viewId == pSliceHeader->nal_ext.mvc.view_id)
            currViewVisited = true;

        for (H264DecoderFrame * pFrm = pDPBList->head(); pFrm && (j < dpbSize + start); pFrm = pFrm->future())
        {
            if (j >= 16)
            {
                VM_ASSERT(false);
                throw h264_exception(UMC_ERR_FAILED);
            }

            VM_ASSERT(j < dpbSize + start);

            int32_t reference = pFrm->isShortTermRef() ? SHORT_TERM_REFERENCE : (pFrm->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);
            if (pFrm->m_viewId != pCurrentFrame->m_viewId && pFrm->m_auIndex == pCurrentFrame->m_auIndex)
            {
                if (pFrm->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_NONE || pFrm->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_NOT_FILLED)
                    continue;

                if (pPicParams_H264->field_pic_flag == 1 && pSliceInfo == pCurrentFrame->GetAU(1))
                {
                    if (!currViewVisited)
                        reference = INTERVIEW_TERM_REFERENCE;

                    if (!reference)
                        reference = INTERVIEW_TERM_REFERENCE;
                }
                else
                {
                    if (!currViewVisited && (pFrm->isInterViewRef(0) || pFrm->isInterViewRef(1)))
                    {
                        reference = INTERVIEW_TERM_REFERENCE;
                    }

                    if (currViewVisited)
                        continue;
                }
            }

            if (!reference)
            {
                continue;
            }

            AddReferenceFrame(pPicParams_H264, j, pFrm, reference);
            if (picParamsIntelMVC)
                picParamsIntelMVC->ViewIDList[j] = (USHORT)pFrm->m_viewId;

            if (picParamsMSMVC)
                picParamsMSMVC->ViewIDList[j] = (USHORT)pFrm->m_viewId;

            if (pFrm == pCurrentFrame && pCurrentFrame->GetAU(0) != pSliceInfo)
            {
                pPicParams_H264->FieldOrderCntList[j][pFrm->GetNumberByParity(1)] = 0;
            }

            if (pPicParams_H264->field_pic_flag == 1 && pFrm->m_auIndex == pCurrentFrame->m_auIndex && pFrm != pCurrentFrame && (pCurrentFrame->GetAU(0) == pSliceInfo || currViewVisited))
            {
                pPicParams_H264->FieldOrderCntList[j][pFrm->GetNumberByParity(1)] = 0;
            }

            j++;
        }
    }

    /*if (m_va->IsMVCSupport() && pSliceHeader->nal_ext.mvc.view_id && picParamsMSMVC)
    {
        uint32_t VOIdx = GetVOIdx(pSlice->m_pSeqParamSetMvcEx, pSliceHeader->nal_ext.mvc.view_id);
        const H264ViewRefInfo &refInfo = pSlice->m_pSeqParamSetMvcEx->viewInfo[VOIdx];

        int32_t numInterViewRefsL0 = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.num_anchor_refs_lx[0] : refInfo.num_non_anchor_refs_lx[0];
        int32_t numInterViewRefsL1 = pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.num_anchor_refs_lx[1] : refInfo.num_non_anchor_refs_lx[1];

        for (int32_t i = 0; i < numInterViewRefsL0 + numInterViewRefsL1; i++)
        {
            int32_t listNum = (i >= numInterViewRefsL0) ?  1 : 0;
            int32_t refNum = listNum ? (i - numInterViewRefsL0) : i;

            ViewItem * view = m_supplier->GetView(pSliceHeader->nal_ext.mvc.anchor_pic_flag ? refInfo.anchor_refs_lx[listNum][refNum] : refInfo.non_anchor_refs_lx[listNum][refNum]);

            if (!view)
                continue;

            H264DecoderFrame * pFrm = view->GetDPBList(0)->findInterViewRef(pCurrentFrame->m_auIndex, pSliceHeader->bottom_field_flag);

            if (!pFrm)
                continue;

            int32_t reference = INTERVIEW_TERM_REFERENCE;
            AddReferenceFrame(pPicParams_H264, j, pFrm, reference);

            if (picParamsIntelMVC)
                picParamsIntelMVC->ViewIDList[j] = (USHORT)pFrm->m_viewId;

            if (picParamsMSMVC)
                picParamsMSMVC->ViewIDList[j] = (USHORT)pFrm->m_viewId;

            j++;
        }
    }*/
}

#ifndef MFX_PROTECTED_FEATURE_DISABLE
void PackerDXVA2::SendPAVPStructure(int32_t numSlicesOfPrevField, H264Slice *pSlice)
{
    mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();
    mfxEncryptedData * encryptedData = bs->EncryptedData;

    // asomsiko: Assuming NalUnitSize calculated correctly if input
    // bitstream contain slice header in clean and slice data placeholder
    // filled with 0xFF.
    // If assumption is not true introduce new field in mfxEncrypedSliceData
    // to store value for NalUnitSize variable to assign it here.
    uint32_t encryptedBufferSize = 0;

    uint32_t ecnryptDataCount = 0;
    if(encryptedData)
    {
        for (int32_t i = 0; i < numSlicesOfPrevField; i++)
        {
            if (!encryptedData)
                throw h264_exception(UMC_ERR_INVALID_PARAMS);

            encryptedData = encryptedData->Next;
        }
        mfxEncryptedData * temp = bs->EncryptedData;
        for (; temp; )
        {
            int size = 0;
            if(temp->Next)
            {
                unsigned long long counter1 = temp->CipherCounter.Count;
                unsigned long long counter2 = temp->Next->CipherCounter.Count;
                unsigned long long zero = 0xffffffffffffffff;
                if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                {
                    counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                    counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                    zero = 0x00000000ffffffff;
                }
                else
                {
                    counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                    counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                }
                if(counter1 <= counter2)
                    size = (uint32_t)(counter2 - counter1) *16;
                else
                    size = (uint32_t)(counter2 + (zero - counter1)) *16;
            }
            else
            {
                size = temp->DataLength + temp->DataOffset;
                if(size & 0xf)
                    size= size + (0x10 - (size & 0xf));
            }

            encryptedBufferSize += size;
            temp = temp->Next;
            ecnryptDataCount++;
        }
        int encryptDataBegin = m_va->GetProtectedVA()->GetBSCurrentEncrypt();

        H264DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        if (pSlice->GetSliceHeader()->field_pic_flag)
        {
            if (numSlicesOfPrevField && ecnryptDataCount < pCurrentFrame->GetAU(0)->GetSliceCount() + pCurrentFrame->GetAU(1)->GetSliceCount())
                throw h264_exception(UMC_ERR_INVALID_PARAMS);
        }
        else
        {
            if (ecnryptDataCount - encryptDataBegin < pCurrentFrame->GetAU(0)->GetSliceCount() + pCurrentFrame->GetAU(1)->GetSliceCount())
                throw h264_exception(UMC_ERR_INVALID_PARAMS);
        }
    }

    HRESULT hr = S_OK;

    DXVA2_DecodeExtensionData  extensionData = {0};
    DXVA_EncryptProtocolHeader extensionOutput = {0};

    extensionData.Function = 0;
    extensionData.pPrivateOutputData = &extensionOutput;
    extensionData.PrivateOutputDataSize = sizeof(extensionOutput);

    DXVA_Intel_Pavp_Protocol2  extensionInput = {0};
    extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
    if(encryptedData)
    {
        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = m_va->GetProtectedVA()->GetEncryptionGUID();
        extensionInput.dwBufferSize = encryptedBufferSize;
        MFX_INTERNAL_CPY(extensionInput.dwAesCounter, &encryptedData->CipherCounter, sizeof(encryptedData->CipherCounter));
    }
    else
    {
        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = DXVA_NoEncrypt;
    }

    extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
    extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();

    extensionData.pPrivateInputData = &extensionInput;
    extensionData.PrivateInputDataSize = sizeof(extensionInput);
    hr = m_va->ExecuteExtensionBuffer(&extensionData);

    if (FAILED(hr) ||
        (encryptedData && extensionOutput.guidEncryptProtocol != m_va->GetProtectedVA()->GetEncryptionGUID()) ||
        // asomsiko: This is workaround for 15.22 driver that starts returning GUID_NULL instead DXVA_NoEncrypt
        /*(!encryptedData && extensionOutput.guidEncryptProtocol != DXVA_NoEncrypt) ||*/
        extensionOutput.dwFunction != 0xffff0801)
    {
        throw h264_exception(UMC_ERR_DEVICE_FAILED);
    }
}

//check correctness of encrypted data
void PackerDXVA2::CheckData()
{
    mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

    if (!bs->EncryptedData) //  no encryption mode
        return;

    mfxEncryptedData * temp = bs->EncryptedData;
    for (; temp; )
    {
        if(temp->Next)
        {
            unsigned long long counter1 = temp->CipherCounter.Count;
            unsigned long long counter2 = temp->Next->CipherCounter.Count;
            unsigned long long zero = 0xffffffffffffffff;
            if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
            {
                counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                zero = 0x00000000ffffffff;
            }
            else
            {
                counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                counter2 = LITTLE_ENDIAN_SWAP64(counter2);
            }
            //second slice offset starts after the first, there is no counter overall
            if(counter1 <= counter2)
            {
                uint32_t size = temp->DataLength + temp->DataOffset;
                if(size & 0xf)
                    size= size+(0x10 - (size & 0xf));
                uint32_t sizeCount = (uint32_t)(counter2 - counter1) *16;
                //if we have hole between slices
                if(sizeCount > size)
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                else //or overlap between slices is different in different buffers
                    if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
            }
            else //there is counter overall or second slice offset starts earlier then the first one
            {
                //length of the second slice offset in 16-bytes blocks
                unsigned long long offsetCounter = (temp->Next->DataOffset + 15)/16;
                // no counter2 overall
                if(zero - counter2 > offsetCounter)
                {
                    uint32_t size = temp->DataLength + temp->DataOffset;
                    if(size & 0xf)
                        size= size+(0x10 - (size & 0xf));
                    unsigned long long counter3 = size / 16;
                    // no zero during the first slice
                    if(zero - counter1 >= counter3)
                    {
                        //second slice offset contains the first slice with offset
                        if(counter2 + offsetCounter >= counter3)
                        {
                            temp->Next->Data += (uint32_t)(counter1 - counter2) * 16;
                            temp->Next->DataOffset -= (uint32_t)(counter1 - counter2) * 16;
                            temp->Next->CipherCounter = temp->CipherCounter;
                            //overlap between slices is different in different buffers
                            if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                                throw h264_exception(UMC_ERR_INVALID_STREAM);
                        }
                        else //overlap between slices data or hole between slices
                        {
                            throw h264_exception(UMC_ERR_INVALID_STREAM);
                        }
                    }
                    else
                    {
                        //size of data between counters
                        uint32_t sizeCount = (uint32_t)(counter2 + (zero - counter1)) *16;
                        //hole between slices
                        if(sizeCount > size)
                            throw h264_exception(UMC_ERR_INVALID_STREAM);
                        else //or overlap between slices is different in different buffers
                            if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                                throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                else
                {
                    temp->Next->Data += (uint32_t)(counter1 - counter2) * 16;
                    temp->Next->DataOffset -= (uint32_t)(counter1 - counter2) * 16;
                    temp->Next->CipherCounter = temp->CipherCounter;
                    //overlap between slices is different in different buffers
                    if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
            }
        }
        temp = temp->Next;
    }
}
#endif // #ifndef MFX_PROTECTED_FEATURE_DISABLE

int32_t PackerDXVA2::PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField)
{
    UMCVACompBuffer* pSliceCompBuf;
    DXVA_Slice_H264_Long* pDXVA_Slice_H264 = (DXVA_Slice_H264_Long*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &pSliceCompBuf);
    if (!m_va->IsLongSliceControl())
    {
        pDXVA_Slice_H264 = (DXVA_Slice_H264_Long*)((DXVA_Slice_H264_Short*)pDXVA_Slice_H264 + sliceNum);
        memset(pDXVA_Slice_H264, 0, sizeof(DXVA_Slice_H264_Short));
    }
    else
    {
        pDXVA_Slice_H264 += sliceNum;
        memset(pDXVA_Slice_H264, 0, sizeof(DXVA_Slice_H264_Long));
    }

    return PackSliceParams(pSlice, sliceNum, chopping, numSlicesOfPrevField, pDXVA_Slice_H264);
}

DXVA_PicEntry_H264 PackerDXVA2::GetFrameIndex(const H264DecoderFrame * frame)
{
    DXVA_PicEntry_H264 idx;
    idx.Index7Bits = frame->m_index;
    return idx;
}

static void CopyWithRedundantElimination(const UMC_H264_DECODER::H264SliceHeader *sliceHdr, uint8_t * dst, uint8_t* src, uint32_t srcSize)
{
    uint32_t first_bit = sliceHdr->hw_wa_redundant_elimination_bits[0];
    uint32_t end_bit = sliceHdr->hw_wa_redundant_elimination_bits[1];

    uint32_t first_byte = first_bit/8;
    uint32_t end_byte = end_bit/8;

    // copy first part:
    MFX_INTERNAL_CPY(dst, src, first_byte);

    dst += first_byte;
    src += first_byte;
    srcSize -= first_byte;

    // copy header size
    first_bit = 8 - (first_bit & 7);
    end_bit = 8 - (end_bit & 7);
    dst[0] = (src[0] >> (first_bit)) << first_bit;

    src += end_byte - first_byte;
    srcSize -= end_byte - first_byte;

    uint32_t shift_bit_count = first_bit - end_bit;

    if (first_byte == end_byte)
    {
        dst[0] |= (src[0] & ((1 << end_bit) - 1)) << shift_bit_count;
        src++;
        srcSize--;
    }
    else
    {
        shift_bit_count = first_bit - (8 - end_bit);
    }

    for (;srcSize > 0;)
    {
        dst[0] = dst[0] | (src[0] >> (8 - shift_bit_count));
        dst[1] = (src[0] << shift_bit_count);
        dst++;
        src++;
        srcSize--;
    }
}

int32_t PackerDXVA2::PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t numSlicesOfPrevField, DXVA_Slice_H264_Long* sliceParams)
{
    static uint8_t start_code_prefix[] = {0, 0, 1};

    int32_t partial_data = CHOPPING_NONE;

    const UMC_H264_DECODER::H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();
    H264DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    uint8_t *pNalUnit; //ptr to first byte of start code
    uint32_t NalUnitSize; // size of NAL unit in byte
    H264HeadersBitstream *pBitstream = pSlice->GetBitStream();

    pBitstream->GetOrg((uint32_t**)&pNalUnit, &NalUnitSize);

    uint8_t *pSliceData; //ptr to slice data
    uint32_t SliceDataOffset; //offset to first bit of slice data from first bit of slice header minus all 0x03
    pBitstream->GetState((uint32_t**)&pSliceData, &SliceDataOffset); //it is supposed that slice header was already parsed

    if (chopping == CHOPPING_SPLIT_SLICE_DATA)
    {
        NalUnitSize = (uint32_t)pBitstream->BytesLeft();
        pNalUnit += pBitstream->BytesDecoded();
        sliceParams->wBadSliceChopping = 2;
    }
    else
    {
        sliceParams->wBadSliceChopping = 0;
    }

    if (!sliceParams->wBadSliceChopping)
        NalUnitSize += sizeof(start_code_prefix);

    uint32_t alignedSize = NalUnitSize;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    mfxBitstream * bs = 0;
    mfxEncryptedData * encryptedData = 0;
    mfxU16 prot = 0;
    if (m_va->GetProtectedVA())
    {
        PAVP_ENCRYPTION_TYPE encrType=(PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
        bs = m_va->GetProtectedVA()->GetBitstream();
        prot = m_va->GetProtectedVA()->GetProtected();

        if (bs->EncryptedData)
        {
            encryptedData = bs->EncryptedData;
            for (int32_t i = 0; i < sliceNum + numSlicesOfPrevField; i++)
            {
                if (!encryptedData)
                    throw h264_exception(UMC_ERR_INVALID_PARAMS);

                encryptedData = encryptedData->Next;
            }

            if(!sliceNum && encrType == PAVP_ENCRYPTION_AES128_CTR)
                CheckData();
        }

        if (!sliceNum && MFX_PROTECTION_PAVP == prot)
        {
            SendPAVPStructure(numSlicesOfPrevField, pSlice);
        }

        if (NULL != encryptedData)
        {
            alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
            if(alignedSize & 0xf)
                alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));
        }
    }
#endif // #ifndef MFX_PROTECTED_FEATURE_DISABLE

    UMCVACompBuffer* CompBuf;
    uint8_t *pDXVA_BitStreamBuffer = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

    if (CompBuf->GetBufferSize() < CompBuf->GetDataSize() + (int32_t)sizeof(start_code_prefix) + (int32_t)alignedSize)
    {
        if (sliceNum - numSlicesOfPrevField > 0)
        {
            return CHOPPING_SPLIT_SLICES;
        }
        else
        {
            alignedSize = CompBuf->GetBufferSize() - CompBuf->GetDataSize();
            NalUnitSize = std::min(NalUnitSize, alignedSize);
            pBitstream->SetDecodedBytes(pBitstream->BytesDecoded() + NalUnitSize);
            if (chopping == CHOPPING_NONE)
                pBitstream->SetDecodedBytes(NalUnitSize - (sliceParams->wBadSliceChopping == 0 ? sizeof(start_code_prefix) : 0));
            //sliceParams->wBadSliceChopping = chopping == CHOPPING_NONE ? 1 : 3;
            //partial_data = CHOPPING_SPLIT_SLICE_DATA;
        }
    }

    sliceParams->BSNALunitDataLocation = CompBuf->GetDataSize();
    pDXVA_BitStreamBuffer += sliceParams->BSNALunitDataLocation;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    uint32_t diff = 0;

    if(encryptedData)
    {
        if(!sliceNum)
        {
            m_pBuf = pDXVA_BitStreamBuffer;
        }
        diff = (uint32_t)(pDXVA_BitStreamBuffer - m_pBuf);
        CompBuf->SetDataSize(sliceParams->BSNALunitDataLocation + (UINT)(alignedSize - diff));
    }
    else
#endif
    {
        CompBuf->SetDataSize(sliceParams->BSNALunitDataLocation + NalUnitSize);
    }

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    if (encryptedData)
    {
        PAVP_ENCRYPTION_TYPE encrType=(PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
        if ((uint32_t)CompBuf->GetBufferSize() < alignedSize)
            throw h264_exception(UMC_ERR_DEVICE_FAILED);

        //location of slice data in buffer
        sliceParams->BSNALunitDataLocation += encryptedData->DataOffset - diff;

        MFX_INTERNAL_CPY(m_pBuf, encryptedData->Data, alignedSize);

        //calculate position of next slice
        if(encryptedData->Next && (encrType == PAVP_ENCRYPTION_AES128_CTR))
        {
            unsigned long long counter1 = encryptedData->CipherCounter.Count;
            unsigned long long counter2 = encryptedData->Next->CipherCounter.Count;
            unsigned long long zero = 0xffffffffffffffff;
            if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
            {
                counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                zero = 0x00000000ffffffff;
            }
            else
            {
                counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                counter2 = LITTLE_ENDIAN_SWAP64(counter2);
            }
            if(counter2 < counter1)
            {
                (unsigned char*)m_pBuf += (uint32_t)(counter2 +(zero - counter1))*16;
            }
            else
            {
                (unsigned char*)m_pBuf += (uint32_t)(counter2 - counter1)*16;
            }
        }

        if (IS_PROTECTION_GPUCP_ANY(prot))
            CompBuf->SetPVPState(&encryptedData->CipherCounter, sizeof (encryptedData->CipherCounter));
        else
            CompBuf->SetPVPState(NULL, 0);
    }
    else
#endif
    {
#ifndef MFX_PROTECTED_FEATURE_DISABLE
        if (m_va->GetProtectedVA())
        {
            if (IS_PROTECTION_GPUCP_ANY(prot))
                CompBuf->SetPVPState(NULL, 0);
        }
#endif

        if (sliceParams->wBadSliceChopping < 2)
        {
            MFX_INTERNAL_CPY(pDXVA_BitStreamBuffer, start_code_prefix, sizeof(start_code_prefix));
            if (!m_va->IsLongSliceControl() && pSlice->GetPicParam()->redundant_pic_cnt_present_flag && (NalUnitSize - sizeof(start_code_prefix) > 0))
            {
                uint32_t srcSize = NalUnitSize;
                uint8_t needToAdujstSize = 0;
                if (m_picParams->entropy_coding_mode_flag) // align slice header
                {
                    srcSize = (pSliceHeader->hw_wa_redundant_elimination_bits[2] + 7) / 8;
                    needToAdujstSize = ((pSliceHeader->hw_wa_redundant_elimination_bits[2] - 1) % 8) == 0;
                }

                CopyWithRedundantElimination(pSliceHeader, pDXVA_BitStreamBuffer + sizeof(start_code_prefix), pNalUnit, srcSize);

                if (m_picParams->entropy_coding_mode_flag)
                {
                    MFX_INTERNAL_CPY(pDXVA_BitStreamBuffer + srcSize + sizeof(start_code_prefix) - needToAdujstSize, pNalUnit + srcSize, NalUnitSize - srcSize);
                }
            }
            else
            {
                if (NalUnitSize - sizeof(start_code_prefix) > 0)
                    MFX_INTERNAL_CPY(pDXVA_BitStreamBuffer + sizeof(start_code_prefix), pNalUnit, NalUnitSize - sizeof(start_code_prefix));
            }
        }
        else
        {
            MFX_INTERNAL_CPY(pDXVA_BitStreamBuffer, pNalUnit, NalUnitSize);
        }
    }

    sliceParams->SliceBytesInBuffer = (UINT)NalUnitSize;

    if (!m_va->IsLongSliceControl())
    {
        return partial_data;
    }

    SliceDataOffset = 31 - SliceDataOffset;
    pSliceData += SliceDataOffset/8;
    SliceDataOffset = (uint32_t)(SliceDataOffset%8 + 8 * (pSliceData - pNalUnit)); //from start code to slice data
    SliceDataOffset -= 8;

    sliceParams->BitOffsetToSliceData = (USHORT)(SliceDataOffset);

    if (sliceParams->wBadSliceChopping == 2 || sliceParams->wBadSliceChopping == 3)
        sliceParams->BitOffsetToSliceData = (USHORT)(0xFFFF);

    sliceParams->NumMbsForSlice = (USHORT)(pSlice->GetMBCount());


    sliceParams->first_mb_in_slice = (USHORT)(pSlice->GetSliceHeader()->first_mb_in_slice >> pSlice->GetSliceHeader()->MbaffFrameFlag);

    sliceParams->slice_type = (UCHAR)(pSliceHeader->slice_type);
    sliceParams->luma_log2_weight_denom = (UCHAR)pSliceHeader->luma_log2_weight_denom;
    sliceParams->chroma_log2_weight_denom = (UCHAR)pSliceHeader->chroma_log2_weight_denom;

    sliceParams->num_ref_idx_l0_active_minus1 = m_picParams->num_ref_idx_l0_active_minus1;
    sliceParams->num_ref_idx_l1_active_minus1 = m_picParams->num_ref_idx_l1_active_minus1;

    if (pSliceHeader->num_ref_idx_active_override_flag != 0)
    {
        sliceParams->num_ref_idx_l0_active_minus1 = (UCHAR)(pSliceHeader->num_ref_idx_l0_active-1);
        if (BPREDSLICE == pSliceHeader->slice_type)
        {
            sliceParams->num_ref_idx_l1_active_minus1 = (UCHAR)(pSliceHeader->num_ref_idx_l1_active-1);
        }
        else
        {
            sliceParams->num_ref_idx_l1_active_minus1 = 0;
        }
    }

    if (pSliceHeader->disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF)
    {
        sliceParams->slice_alpha_c0_offset_div2 = (CHAR)(pSliceHeader->slice_alpha_c0_offset / 2);
        sliceParams->slice_beta_offset_div2 = (CHAR)(pSliceHeader->slice_beta_offset / 2);
    }

    //RefPicList
    int32_t realSliceNum = pSlice->GetSliceNum();
    if (pCurrentFrame == nullptr)
        throw h264_exception(UMC_ERR_FAILED);

    const H264DecoderRefPicList* pH264DecRefPicList0 = pCurrentFrame->GetRefPicList(realSliceNum, 0);
    const H264DecoderRefPicList* pH264DecRefPicList1 = pCurrentFrame->GetRefPicList(realSliceNum, 1);

    if (pH264DecRefPicList0 == nullptr || pH264DecRefPicList1 == nullptr)
        throw h264_exception(UMC_ERR_FAILED);

    H264DecoderFrame **pRefPicList0 = pH264DecRefPicList0->m_RefPicList;
    H264DecoderFrame **pRefPicList1 = pH264DecRefPicList1->m_RefPicList;
    ReferenceFlags *pFields0 = pH264DecRefPicList0->m_Flags;
    ReferenceFlags *pFields1 = pH264DecRefPicList1->m_Flags;

    for (int32_t i = 0; i < 32; i++)
    {
        if (pRefPicList0[i] != NULL && i < pSliceHeader->num_ref_idx_l0_active)
        {
            DXVA_PicEntry_H264 idx = GetFrameIndex(pRefPicList0[i]);
            uint16_t frameNum = pFields0[i].isLongReference ? (uint16_t)pRefPicList0[i]->m_LongTermFrameIdx : (uint16_t)pRefPicList0[i]->m_FrameNum;;
            bool isFound = false;
            for(int32_t j = 0; j < 16; j++) //index in RefFrameList array
            {
                if (idx.Index7Bits == m_picParams->RefFrameList[j].Index7Bits &&
                    m_picParams->FrameNumList[j] == frameNum)
                {
                    isFound = true;
                    sliceParams->RefPicList[0][i].Index7Bits = (UCHAR)j;
                    sliceParams->RefPicList[0][i].AssociatedFlag = pFields0[i].field;
                    break;
                }
            }

            VM_ASSERT(isFound || !pRefPicList0[i]->IsFrameExist());
        }
        else
            sliceParams->RefPicList[0][i].bPicEntry = 0xff;

        if (pRefPicList1[i] != NULL && i < pSliceHeader->num_ref_idx_l1_active)
        {
            DXVA_PicEntry_H264 idx = GetFrameIndex(pRefPicList1[i]);
            uint16_t frameNum = pFields1[i].isLongReference ? (uint16_t)pRefPicList1[i]->m_LongTermFrameIdx : (uint16_t)pRefPicList1[i]->m_FrameNum;;
            bool isFound = false;
            for (int32_t j = 0; j < 16; j++)   //index in RefFrameList array
            {
                if (idx.Index7Bits == m_picParams->RefFrameList[j].Index7Bits &&
                    m_picParams->FrameNumList[j] == frameNum)
                {
                    isFound = true;
                    sliceParams->RefPicList[1][i].Index7Bits = (UCHAR)j;
                    sliceParams->RefPicList[1][i].AssociatedFlag = pFields1[i].field;
                    break;
                }
            }
            VM_ASSERT(isFound || !pRefPicList1[i]->IsFrameExist());
        }
        else
            sliceParams->RefPicList[1][i].bPicEntry = 0xff;
    }

    if ( (m_picParams->weighted_pred_flag &&
          ((PREDSLICE == pSliceHeader->slice_type) || (S_PREDSLICE == pSliceHeader->slice_type))) ||
         ((m_picParams->weighted_bipred_idc == 1) && (BPREDSLICE == pSliceHeader->slice_type)))
    {
        //Weights
        const UMC_H264_DECODER::PredWeightTable *pPredWeight[2];
        pPredWeight[0] = pSlice->GetPredWeigthTable(0);
        pPredWeight[1] = pSlice->GetPredWeigthTable(1);

        for(int32_t j = 0; j < 2; j++)
        {
            int32_t active_number = j == 0 ? pSliceHeader->num_ref_idx_l0_active : pSliceHeader->num_ref_idx_l1_active;
            for(int32_t i = 0; i < active_number; i++)
            {
                if (pPredWeight[j][i].luma_weight_flag)
                {
                    sliceParams->Weights[j][i][0][0] = pPredWeight[j][i].luma_weight;
                    sliceParams->Weights[j][i][0][1] = pPredWeight[j][i].luma_offset;
                }
                else
                {
                    sliceParams->Weights[j][i][0][0] = (uint8_t)pPredWeight[j][i].luma_weight;
                    sliceParams->Weights[j][i][0][1] = 0;
                }

                if (pPredWeight[j][i].chroma_weight_flag)
                {
                    sliceParams->Weights[j][i][1][0] = pPredWeight[j][i].chroma_weight[0];
                    sliceParams->Weights[j][i][1][1] = pPredWeight[j][i].chroma_offset[0];
                    sliceParams->Weights[j][i][2][0] = pPredWeight[j][i].chroma_weight[1];
                    sliceParams->Weights[j][i][2][1] = pPredWeight[j][i].chroma_offset[1];
                }
                else
                {
                    sliceParams->Weights[j][i][1][0] = (uint8_t)pPredWeight[j][i].chroma_weight[0];
                    sliceParams->Weights[j][i][1][1] = 0;
                    sliceParams->Weights[j][i][2][0] = (uint8_t)pPredWeight[j][i].chroma_weight[1];
                    sliceParams->Weights[j][i][2][1] = 0;
                }
            }

            if ((PREDSLICE == pSliceHeader->slice_type) || (S_PREDSLICE == pSliceHeader->slice_type))
                break;
        }
    }

    sliceParams->slice_qs_delta = (CHAR)pSliceHeader->slice_qs_delta;
    sliceParams->disable_deblocking_filter_idc =  (UCHAR)pSliceHeader->disable_deblocking_filter_idc;

    sliceParams->slice_qp_delta = (CHAR)pSliceHeader->slice_qp_delta;
    sliceParams->redundant_pic_cnt = (UCHAR)pSliceHeader->redundant_pic_cnt;
    sliceParams->direct_spatial_mv_pred_flag = pSliceHeader->direct_spatial_mv_pred_flag;
    sliceParams->cabac_init_idc = (UCHAR)pSliceHeader->cabac_init_idc;
    sliceParams->slice_id = (USHORT)(pSlice->GetSliceNum() - 1);

    return partial_data;
}

void PackerDXVA2::BeginFrame(H264DecoderFrame*, int32_t)
{
    m_statusReportFeedbackCounter++;
    // WA: StatusReportFeedbackNumber - UINT, can't be 0 - reported status will be ignored
    if (m_statusReportFeedbackCounter >= UINT_MAX)
        m_statusReportFeedbackCounter = 1;
    m_picParams = 0;
}

void PackerDXVA2::EndFrame()
{
    m_picParams = 0;
}

void PackerDXVA2::PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling)
{
    UMCVACompBuffer *quantBuf;
    DXVA_Qmatrix_H264* pQmatrix_H264 = (DXVA_Qmatrix_H264*)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &quantBuf);

    quantBuf->SetDataSize(sizeof(DXVA_Qmatrix_H264));

    static int32_t inv_mp_scan4x4[16] =
    {
        0,  1,  5,  6,
        2,  4,  7,  12,
        3,  8,  11, 13,
        9,  10, 14, 15
    };

    static int32_t inv_hp[64] =
    {
        0, 1, 5, 6, 14, 15, 27, 28,
        2, 4, 7, 13, 16, 26, 29, 42,
        3, 8, 12, 17, 25, 30, 41, 43,
        9, 11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    };

    for(int32_t j = 0; j < 6; j++)
    {
        for(int32_t i = 0; i < 16; i++)
        {
             pQmatrix_H264->bScalingLists4x4[j][inv_mp_scan4x4[i]] = scaling->ScalingLists4x4[j].ScalingListCoeffs[i];
        }
    }

    for(int32_t j = 0; j < 2; j++)
    {
        for(int32_t i = 0; i < 64; i++)
        {
             pQmatrix_H264->bScalingLists8x8[j][inv_hp[i]] = scaling->ScalingLists8x8[j].ScalingListCoeffs[i];
        }
    }
}

#endif // UMC_VA_DXVA


#ifdef UMC_VA_LINUX
/****************************************************************************************************/
// VA linux packer implementation
/****************************************************************************************************/
PackerVA::PackerVA(VideoAccelerator * va, TaskSupplier * supplier)
    : Packer(va, supplier)
{
}

Status PackerVA::GetStatusReport(void * pStatusReport, size_t size)
{
    (void)pStatusReport;
    (void)size;

    return UMC_OK;
}

void PackerVA::FillFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
                         int32_t field, int32_t reference, int32_t defaultIndex)
{
    int32_t index = pFrame->m_index;

    if (index == -1)
        index = defaultIndex;

    pic->picture_id = m_va->GetSurfaceID(index);
    pic->frame_idx = pFrame->isLongTermRef() ? (uint16_t)pFrame->m_LongTermFrameIdx : (uint16_t)pFrame->m_FrameNum;

    int parityNum0 = pFrame->GetNumberByParity(0);
    if (parityNum0 >= 0 && parityNum0 < 2)
    {
        pic->TopFieldOrderCnt = pFrame->m_PicOrderCnt[parityNum0];
    }
    else
    {
        VM_ASSERT(0);
    }
    int parityNum1 = pFrame->GetNumberByParity(1);
    if (parityNum1 >= 0 && parityNum1 < 2)
    {
        pic->BottomFieldOrderCnt = pFrame->m_PicOrderCnt[parityNum1];
    }
    else
    {
        VM_ASSERT(0);
    }
    pic->flags = 0;

    if (pFrame->m_PictureStructureForDec == 0)
    {
        pic->flags |= field ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
    }

    if (reference == 1)
        pic->flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;

    if (reference == 2)
        pic->flags |= VA_PICTURE_H264_LONG_TERM_REFERENCE;

    if (pic->picture_id == VA_INVALID_ID)
    {
        pic->frame_idx = 0;
        pic->TopFieldOrderCnt = 0;
        pic->BottomFieldOrderCnt = 0;
        pic->flags = VA_PICTURE_H264_INVALID;
    }
}

int32_t PackerVA::FillRefFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
                            ReferenceFlags flags, bool isField, int32_t defaultIndex)
{
    int32_t index = pFrame->m_index;

    if (index == -1)
        index = defaultIndex;

    pic->picture_id = m_va->GetSurfaceID(index);
    pic->frame_idx = pFrame->isLongTermRef() ? (uint16_t)pFrame->m_LongTermFrameIdx : (uint16_t)pFrame->m_FrameNum;

    int parityNum0 = pFrame->GetNumberByParity(0);
    if (parityNum0 >= 0 && parityNum0 < 2)
    {
        pic->TopFieldOrderCnt = pFrame->m_PicOrderCnt[parityNum0];
    }
    else
    {
        VM_ASSERT(0);
    }
    int parityNum1 = pFrame->GetNumberByParity(1);
    if (parityNum1 >= 0 && parityNum1 < 2)
    {
        pic->BottomFieldOrderCnt = pFrame->m_PicOrderCnt[parityNum1];
    }
    else
    {
        VM_ASSERT(0);
    }

    pic->flags = 0;

    if (isField)
    {
        pic->flags |= flags.field ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
    }

    pic->flags |= flags.isShortReference ? VA_PICTURE_H264_SHORT_TERM_REFERENCE : VA_PICTURE_H264_LONG_TERM_REFERENCE;

    if (pic->picture_id == VA_INVALID_ID)
    {
        pic->frame_idx = 0;
        pic->TopFieldOrderCnt = 0;
        pic->BottomFieldOrderCnt = 0;
        pic->flags = VA_PICTURE_H264_INVALID;
    }

    return pic->picture_id;
}

void PackerVA::FillFrameAsInvalid(VAPictureH264 * pic)
{
    pic->picture_id = VA_INVALID_SURFACE;
    pic->frame_idx = 0;
    pic->TopFieldOrderCnt = 0;
    pic->BottomFieldOrderCnt = 0;
    pic->flags = VA_PICTURE_H264_INVALID;
}


void PackerVA::PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice)
{
    const UMC_H264_DECODER::H264SliceHeader* pSliceHeader = pSlice->GetSliceHeader();
    const UMC_H264_DECODER::H264SeqParamSet* pSeqParamSet = pSlice->GetSeqParam();
    const UMC_H264_DECODER::H264PicParamSet* pPicParamSet = pSlice->GetPicParam();

    const H264DecoderFrame *pCurrentFrame = pSliceInfo->m_pFrame;

    UMCVACompBuffer *picParamBuf;
    VAPictureParameterBufferH264* pPicParams_H264 = (VAPictureParameterBufferH264*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferH264));
    if (!pPicParams_H264)
        throw h264_exception(UMC_ERR_FAILED);

    memset(pPicParams_H264, 0, sizeof(VAPictureParameterBufferH264));

    int32_t reference = pCurrentFrame->isShortTermRef() ? 1 : (pCurrentFrame->isLongTermRef() ? 2 : 0);

    FillFrame(&(pPicParams_H264->CurrPic), pCurrentFrame, pSliceHeader->bottom_field_flag, reference, 0);

    pPicParams_H264->CurrPic.flags = 0;

    if (reference == 1)
        pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;

    if (reference == 2)
        pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_LONG_TERM_REFERENCE;

    if (pSliceHeader->field_pic_flag)
    {
        if (pSliceHeader->bottom_field_flag)
        {
            pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_BOTTOM_FIELD;
            pPicParams_H264->CurrPic.TopFieldOrderCnt = 0;
        }
        else
        {
            pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_TOP_FIELD;
            pPicParams_H264->CurrPic.BottomFieldOrderCnt = 0;
        }
    }

    //packing
    pPicParams_H264->picture_width_in_mbs_minus1 = (unsigned short)(pSeqParamSet->frame_width_in_mbs - 1);
    pPicParams_H264->picture_height_in_mbs_minus1 = (unsigned short)(pSeqParamSet->frame_height_in_mbs - 1);

    pPicParams_H264->bit_depth_luma_minus8 = (unsigned char)(pSeqParamSet->bit_depth_luma - 8);
    pPicParams_H264->bit_depth_chroma_minus8 = (unsigned char)(pSeqParamSet->bit_depth_chroma - 8);

    pPicParams_H264->num_ref_frames = (unsigned char)pSeqParamSet->num_ref_frames;

    pPicParams_H264->seq_fields.bits.chroma_format_idc = pSeqParamSet->chroma_format_idc;
    pPicParams_H264->seq_fields.bits.residual_colour_transform_flag = pSeqParamSet->residual_colour_transform_flag;
    //pPicParams_H264->seq_fields.bits.gaps_in_frame_num_value_allowed_flag = ???
    pPicParams_H264->seq_fields.bits.frame_mbs_only_flag = pSeqParamSet->frame_mbs_only_flag;
    pPicParams_H264->seq_fields.bits.mb_adaptive_frame_field_flag = pSliceHeader->MbaffFrameFlag;
    pPicParams_H264->seq_fields.bits.direct_8x8_inference_flag = pSeqParamSet->direct_8x8_inference_flag;
    pPicParams_H264->seq_fields.bits.MinLumaBiPredSize8x8 = pSeqParamSet->level_idc > 30 ? 1 : 0;
    pPicParams_H264->seq_fields.bits.log2_max_frame_num_minus4 = (unsigned char)(pSeqParamSet->log2_max_frame_num - 4);
    pPicParams_H264->seq_fields.bits.pic_order_cnt_type = pSeqParamSet->pic_order_cnt_type;
    pPicParams_H264->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = (unsigned char)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParams_H264->seq_fields.bits.delta_pic_order_always_zero_flag = pSeqParamSet->delta_pic_order_always_zero_flag;

    // pPicParams_H264->num_slice_groups_minus1 = (unsigned char)(pPicParamSet->num_slice_groups - 1);
    // pPicParams_H264->slice_group_map_type = (unsigned char)pPicParamSet->SliceGroupInfo.slice_group_map_type;
    pPicParams_H264->pic_init_qp_minus26 = (unsigned char)(pPicParamSet->pic_init_qp - 26);
    pPicParams_H264->pic_init_qs_minus26 = (unsigned char)(pPicParamSet->pic_init_qs - 26);
    pPicParams_H264->chroma_qp_index_offset = (unsigned char)pPicParamSet->chroma_qp_index_offset[0];
    pPicParams_H264->second_chroma_qp_index_offset = (unsigned char)pPicParamSet->chroma_qp_index_offset[1];

    pPicParams_H264->pic_fields.bits.entropy_coding_mode_flag = pPicParamSet->entropy_coding_mode;
    pPicParams_H264->pic_fields.bits.weighted_pred_flag = pPicParamSet->weighted_pred_flag;
    pPicParams_H264->pic_fields.bits.weighted_bipred_idc = pPicParamSet->weighted_bipred_idc;
    pPicParams_H264->pic_fields.bits.transform_8x8_mode_flag = pPicParamSet->transform_8x8_mode_flag;
    pPicParams_H264->pic_fields.bits.field_pic_flag = pSliceHeader->field_pic_flag;
    pPicParams_H264->pic_fields.bits.constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
    pPicParams_H264->pic_fields.bits.pic_order_present_flag = pPicParamSet->bottom_field_pic_order_in_frame_present_flag;
    pPicParams_H264->pic_fields.bits.deblocking_filter_control_present_flag = pPicParamSet->deblocking_filter_variables_present_flag;
    pPicParams_H264->pic_fields.bits.redundant_pic_cnt_present_flag = 0;//pPicParamSet->redundant_pic_cnt_present_flag;
    pPicParams_H264->pic_fields.bits.reference_pic_flag = pSliceHeader->nal_ref_idc != 0; //!!!

    pPicParams_H264->frame_num = (unsigned short)pSliceHeader->frame_num;

//    pPicParams_H264->num_ref_idx_l0_default_active_minus1 = (unsigned char)(pPicParamSet->num_ref_idx_l0_active-1);
//    pPicParams_H264->num_ref_idx_l1_default_active_minus1 = (unsigned char)(pPicParamSet->num_ref_idx_l1_active-1);

    //create reference picture list
    for (int32_t i = 0; i < 16; i++)
    {
        FillFrameAsInvalid(&(pPicParams_H264->ReferenceFrames[i]));
    }

    int32_t referenceCount = 0;
    int32_t j = 0;

    int32_t viewCount = m_supplier->GetViewCount();

    for (int32_t i = 0; i < viewCount; i++)
    {
        ViewItem & view = m_supplier->GetViewByNumber(i);
        H264DBPList * pDPBList = view.GetDPBList(0);
        int32_t dpbSize = pDPBList->GetDPBSize();

        int32_t start = j;

        for (H264DecoderFrame * pFrm = pDPBList->head(); pFrm && (j < dpbSize + start); pFrm = pFrm->future())
        {
            if (j >= 16)
            {
                VM_ASSERT(false);
                throw h264_exception(UMC_ERR_FAILED);
            }
            VM_ASSERT(j < dpbSize + start);

            int32_t defaultIndex = 0;

            if ((0 == pCurrentFrame->m_index) && !pFrm->IsFrameExist())
            {
                defaultIndex = 1;
            }

            int32_t reference = pFrm->isShortTermRef() ? 1 : (pFrm->isLongTermRef() ? 2 : 0);
            if (!reference && pCurrentFrame != pFrm && (pFrm->isInterViewRef(0) || pFrm->isInterViewRef(1)) &&
                (pFrm->PicOrderCnt(0, 3) == pCurrentFrame->PicOrderCnt(0, 3)) && pFrm->m_viewId < pCurrentFrame->m_viewId)
            { // interview reference
                reference = 1;
            }

            if (!reference)
            {
                continue;
            }

            reference = pFrm->isShortTermRef() ? 1 : (pFrm->isLongTermRef() ? 2 : 0);
            referenceCount ++;
            int32_t field = pFrm->m_bottom_field_flag[0];
            FillFrame(&(pPicParams_H264->ReferenceFrames[j]), pFrm,
                field, reference, defaultIndex);

            reference = pFrm->isShortTermRef() ? 1 : (pFrm->isLongTermRef() ? 2 : 0);

            if ((pFrm == pCurrentFrame) && ((&pCurrentFrame->m_pSlicesInfo) != pSliceInfo))
            {
                FillFrame(&(pPicParams_H264->ReferenceFrames[j]), pFrm, 0, reference, defaultIndex);
            }

            j++;
        }
    }

    picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferH264));
}


//returns both NAL unit size (in bytes) and bit offset from start to actual slice data
inline
uint8_t* GetSliceStat(H264Slice* slice, uint32_t* size, uint32_t* offset)
{
    VM_ASSERT(slice);
    VM_ASSERT(size);

    H264HeadersBitstream* bs = slice->GetBitStream();
    VM_ASSERT(bs);

    uint8_t* base;   //ptr to first byte of start code
    bs->GetOrg(reinterpret_cast<uint32_t**>(&base), size);

    if (offset)
    {
        uint8_t* ptr;    //ptr to slice data
        uint32_t position;
        bs->GetState(reinterpret_cast<uint32_t**>(&ptr), &position);

        VM_ASSERT(base != ptr &&
                  !"slice header should be already parsed here"
        );

        //GetState returns internal offset (bits left) but we need consumed bits
        position = 31 - position;
        //bit from start code to slice data
        position += 8 * (ptr - base);

        *offset = position;
    }

    return base;
}

void PackerVA::CreateSliceParamBuffer(H264DecoderFrameInfo * pSliceInfo)
{
    int32_t count = pSliceInfo->GetSliceCount();

    UMCVACompBuffer *pSliceParamBuf;
    size_t sizeOfStruct = sizeof(VASliceParameterBufferH264);

    if (!m_va->IsLongSliceControl())
    {
        sizeOfStruct = sizeof(VASliceParameterBufferBase);
    }
    m_va->GetCompBuffer(VASliceParameterBufferType, &pSliceParamBuf, sizeOfStruct*(count));
    if (!pSliceParamBuf)
        throw h264_exception(UMC_ERR_FAILED);

    pSliceParamBuf->SetNumOfItem(count);
}

void PackerVA::CreateSliceDataBuffer(H264DecoderFrameInfo * pSliceInfo)
{
    int32_t count = pSliceInfo->GetSliceCount();

    uint32_t size = 0;
    for (int32_t i = 0; i < count; i++)
    {
        H264Slice* pSlice = pSliceInfo->GetSlice(i);
        uint32_t NalUnitSize;
        GetSliceStat(pSlice, &NalUnitSize, 0);

        size += NalUnitSize;
    }

    uint32_t const AlignedNalUnitSize = mfx::align2_value(size, 128);

    UMCVACompBuffer* compBuf;
    m_va->GetCompBuffer(VASliceDataBufferType, &compBuf, AlignedNalUnitSize);
    if (!compBuf)
        throw h264_exception(UMC_ERR_FAILED);

    memset((uint8_t*)compBuf->GetPtr() + size, 0, AlignedNalUnitSize - size);

    compBuf->SetDataSize(0);
}

int32_t PackerVA::PackSliceParams(H264Slice *pSlice, int32_t sliceNum, int32_t chopping, int32_t )
{
    int32_t partial_data = CHOPPING_NONE;
    H264DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
    if (pCurrentFrame == nullptr)
        throw h264_exception(UMC_ERR_FAILED);
    const UMC_H264_DECODER::H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    VAPictureParameterBufferH264* pPicParams_H264 = (VAPictureParameterBufferH264*)m_va->GetCompBuffer(VAPictureParameterBufferType);
    if (!pPicParams_H264)
        throw h264_exception(UMC_ERR_FAILED);

    UMCVACompBuffer* compBuf;
    VASliceParameterBufferH264* pSlice_H264 = (VASliceParameterBufferH264*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBuf);
    if (!pSlice_H264)
        throw h264_exception(UMC_ERR_FAILED);

    if (m_va->IsLongSliceControl())
    {
        pSlice_H264 += sliceNum;
        memset(pSlice_H264, 0, sizeof(VASliceParameterBufferH264));
    }
    else
    {
        pSlice_H264 = (VASliceParameterBufferH264*)((VASliceParameterBufferBase*)pSlice_H264 + sliceNum);
        memset(pSlice_H264, 0, sizeof(VASliceParameterBufferBase));
    }

    uint32_t NalUnitSize, SliceDataOffset;
    uint8_t* pNalUnit = GetSliceStat(pSlice, &NalUnitSize, &SliceDataOffset);
    if (SliceDataOffset >= NalUnitSize * 8)
        //no slice data, skipping
        return CHOPPING_SKIP_SLICE;

    H264HeadersBitstream* pBitstream = pSlice->GetBitStream();
    if (chopping == CHOPPING_SPLIT_SLICE_DATA)
    {
        NalUnitSize = pBitstream->BytesLeft();
        pNalUnit += pBitstream->BytesDecoded();
    }

    UMCVACompBuffer* CompBuf;
    uint8_t *pVAAPI_BitStreamBuffer = (uint8_t*)m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf);
    if (!pVAAPI_BitStreamBuffer)
        throw h264_exception(UMC_ERR_FAILED);

    int32_t AlignedNalUnitSize = NalUnitSize;

    pSlice_H264->slice_data_flag = chopping == CHOPPING_NONE ? VA_SLICE_DATA_FLAG_ALL : VA_SLICE_DATA_FLAG_END;

    if (CompBuf->GetBufferSize() - CompBuf->GetDataSize() < AlignedNalUnitSize)
    {
        AlignedNalUnitSize = NalUnitSize = CompBuf->GetBufferSize() - CompBuf->GetDataSize();
        pBitstream->SetDecodedBytes(pBitstream->BytesDecoded() + NalUnitSize);
        pSlice_H264->slice_data_flag = chopping == CHOPPING_NONE ? VA_SLICE_DATA_FLAG_BEGIN : VA_SLICE_DATA_FLAG_MIDDLE;
        partial_data = CHOPPING_SPLIT_SLICE_DATA;
    }

    pSlice_H264->slice_data_size = NalUnitSize;

    pSlice_H264->slice_data_offset = CompBuf->GetDataSize();
    CompBuf->SetDataSize(pSlice_H264->slice_data_offset + AlignedNalUnitSize);

    VM_ASSERT (CompBuf->GetBufferSize() >= pSlice_H264->slice_data_offset + AlignedNalUnitSize);

    pVAAPI_BitStreamBuffer += pSlice_H264->slice_data_offset;

    std::copy(pNalUnit, pNalUnit + NalUnitSize, pVAAPI_BitStreamBuffer);
    memset(pVAAPI_BitStreamBuffer + NalUnitSize, 0, AlignedNalUnitSize - NalUnitSize);

    if (!m_va->IsLongSliceControl())
        return partial_data;

    pSlice_H264->slice_data_bit_offset = (unsigned short)SliceDataOffset;

    pSlice_H264->first_mb_in_slice = (unsigned short)(pSlice->GetSliceHeader()->first_mb_in_slice >> pSlice->GetSliceHeader()->MbaffFrameFlag);
    pSlice_H264->slice_type = (unsigned char)pSliceHeader->slice_type;
    pSlice_H264->direct_spatial_mv_pred_flag = (unsigned char)pSliceHeader->direct_spatial_mv_pred_flag;
    pSlice_H264->cabac_init_idc = (unsigned char)(pSliceHeader->cabac_init_idc);
    pSlice_H264->slice_qp_delta = (char)pSliceHeader->slice_qp_delta;
    pSlice_H264->disable_deblocking_filter_idc = (unsigned char)pSliceHeader->disable_deblocking_filter_idc;
    pSlice_H264->luma_log2_weight_denom = (unsigned char)pSliceHeader->luma_log2_weight_denom;
    pSlice_H264->chroma_log2_weight_denom = (unsigned char)pSliceHeader->chroma_log2_weight_denom;

    if (pSliceHeader->slice_type == INTRASLICE ||
        pSliceHeader->slice_type == S_INTRASLICE)
    {
        pSlice_H264->num_ref_idx_l0_active_minus1 = 0;
        pSlice_H264->num_ref_idx_l1_active_minus1 = 0;
    }
    else if (pSliceHeader->slice_type == PREDSLICE ||
        pSliceHeader->slice_type == S_PREDSLICE)
    {
        if (pSliceHeader->num_ref_idx_active_override_flag != 0)
        {
            pSlice_H264->num_ref_idx_l0_active_minus1 = (unsigned char)(pSliceHeader->num_ref_idx_l0_active-1);
        }
        else
        {
            pSlice_H264->num_ref_idx_l0_active_minus1 = (unsigned char)(pSlice->GetPicParam()->num_ref_idx_l0_active - 1);
        }
        pSlice_H264->num_ref_idx_l1_active_minus1 = 0;
    }
    else // B slice
    {
        if (pSliceHeader->num_ref_idx_active_override_flag != 0)
        {
            pSlice_H264->num_ref_idx_l0_active_minus1 = (unsigned char)(pSliceHeader->num_ref_idx_l0_active - 1);
            pSlice_H264->num_ref_idx_l1_active_minus1 = (unsigned char)(pSliceHeader->num_ref_idx_l1_active-1);
        }
        else
        {
            pSlice_H264->num_ref_idx_l0_active_minus1 = (unsigned char)(pSlice->GetPicParam()->num_ref_idx_l0_active - 1);
            pSlice_H264->num_ref_idx_l1_active_minus1 = (unsigned char)(pSlice->GetPicParam()->num_ref_idx_l1_active - 1);
        }
    }

    if (pSliceHeader->disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF)
    {
        pSlice_H264->slice_alpha_c0_offset_div2 = (char)(pSliceHeader->slice_alpha_c0_offset / 2);
        pSlice_H264->slice_beta_offset_div2 = (char)(pSliceHeader->slice_beta_offset / 2);
    }

    if ((pPicParams_H264->pic_fields.bits.weighted_pred_flag &&
         ((PREDSLICE == pSliceHeader->slice_type) || (S_PREDSLICE == pSliceHeader->slice_type))) ||
         ((pPicParams_H264->pic_fields.bits.weighted_bipred_idc == 1) && (BPREDSLICE == pSliceHeader->slice_type)))
    {
        //Weights
        const UMC_H264_DECODER::PredWeightTable *pPredWeight[2];
        pPredWeight[0] = pSlice->GetPredWeigthTable(0);
        pPredWeight[1] = pSlice->GetPredWeigthTable(1);

        int32_t  i;
        for(i=0; i < 32; i++)
        {
            if (pPredWeight[0][i].luma_weight_flag)
            {
                pSlice_H264->luma_weight_l0[i] = pPredWeight[0][i].luma_weight;
                pSlice_H264->luma_offset_l0[i] = pPredWeight[0][i].luma_offset;
            }
            else
            {
                pSlice_H264->luma_weight_l0[i] = (uint8_t)pPredWeight[0][i].luma_weight;
                pSlice_H264->luma_offset_l0[i] = 0;
            }
            if (pPredWeight[1][i].luma_weight_flag)
            {
                pSlice_H264->luma_weight_l1[i] = pPredWeight[1][i].luma_weight;
                pSlice_H264->luma_offset_l1[i] = pPredWeight[1][i].luma_offset;
            }
            else
            {
                pSlice_H264->luma_weight_l1[i] = (uint8_t)pPredWeight[1][i].luma_weight;
                pSlice_H264->luma_offset_l1[i] = 0;
            }
            if (pPredWeight[0][i].chroma_weight_flag)
            {
                pSlice_H264->chroma_weight_l0[i][0] = pPredWeight[0][i].chroma_weight[0];
                pSlice_H264->chroma_offset_l0[i][0] = pPredWeight[0][i].chroma_offset[0];
                pSlice_H264->chroma_weight_l0[i][1] = pPredWeight[0][i].chroma_weight[1];
                pSlice_H264->chroma_offset_l0[i][1] = pPredWeight[0][i].chroma_offset[1];
            }
            else
            {
                pSlice_H264->chroma_weight_l0[i][0] = (uint8_t)pPredWeight[0][i].chroma_weight[0];
                pSlice_H264->chroma_offset_l0[i][0] = 0;
                pSlice_H264->chroma_weight_l0[i][1] = (uint8_t)pPredWeight[0][i].chroma_weight[1];
                pSlice_H264->chroma_offset_l0[i][1] = 0;
            }
            if (pPredWeight[1][i].chroma_weight_flag)
            {
                pSlice_H264->chroma_weight_l1[i][0] = pPredWeight[1][i].chroma_weight[0];
                pSlice_H264->chroma_offset_l1[i][0] = pPredWeight[1][i].chroma_offset[0];
                pSlice_H264->chroma_weight_l1[i][1] = pPredWeight[1][i].chroma_weight[1];
                pSlice_H264->chroma_offset_l1[i][1] = pPredWeight[1][i].chroma_offset[1];
            }
            else
            {
                pSlice_H264->chroma_weight_l1[i][0] = (uint8_t)pPredWeight[1][i].chroma_weight[0];
                pSlice_H264->chroma_offset_l1[i][0] = 0;
                pSlice_H264->chroma_weight_l1[i][1] = (uint8_t)pPredWeight[1][i].chroma_weight[1];
                pSlice_H264->chroma_offset_l1[i][1] = 0;
            }
        }
    }

    int32_t realSliceNum = pSlice->GetSliceNum();

    const H264DecoderRefPicList* pH264DecRefPicList0 = pCurrentFrame->GetRefPicList(realSliceNum, 0);
    const H264DecoderRefPicList* pH264DecRefPicList1 = pCurrentFrame->GetRefPicList(realSliceNum, 1);

    if (pH264DecRefPicList0 == nullptr || pH264DecRefPicList1 == nullptr)
        throw h264_exception(UMC_ERR_FAILED);

    H264DecoderFrame **pRefPicList0 = pH264DecRefPicList0->m_RefPicList;
    H264DecoderFrame **pRefPicList1 = pH264DecRefPicList1->m_RefPicList;
    ReferenceFlags *pFields0 = pH264DecRefPicList0->m_Flags;
    ReferenceFlags *pFields1 = pH264DecRefPicList1->m_Flags;

    int32_t i;
    for(i = 0; i < 32; i++)
    {
        if (pRefPicList0[i] != NULL && i < pSliceHeader->num_ref_idx_l0_active)
        {
            int32_t defaultIndex = ((0 == pCurrentFrame->m_index) && !pRefPicList0[i]->IsFrameExist()) ? 1 : 0;

            FillRefFrame(&(pSlice_H264->RefPicList0[i]), pRefPicList0[i],
                pFields0[i], pSliceHeader->field_pic_flag, defaultIndex);

            if (pSlice_H264->RefPicList0[i].picture_id == pPicParams_H264->CurrPic.picture_id &&
                pRefPicList0[i]->IsFrameExist())
            {
                pSlice_H264->RefPicList0[i].BottomFieldOrderCnt = 0;
            }
        }
        else
        {
            FillFrameAsInvalid(&(pSlice_H264->RefPicList0[i]));
        }

        if (pRefPicList1[i] != NULL && i < pSliceHeader->num_ref_idx_l1_active)
        {
            int32_t defaultIndex = ((0 == pCurrentFrame->m_index) && !pRefPicList1[i]->IsFrameExist()) ? 1 : 0;

            FillRefFrame(&(pSlice_H264->RefPicList1[i]), pRefPicList1[i],
                pFields1[i], pSliceHeader->field_pic_flag, defaultIndex);

            if (pSlice_H264->RefPicList1[i].picture_id == pPicParams_H264->CurrPic.picture_id && pRefPicList1[i]->IsFrameExist())
            {
                pSlice_H264->RefPicList1[i].BottomFieldOrderCnt = 0;
            }
        }
        else
        {
            FillFrameAsInvalid(&(pSlice_H264->RefPicList1[i]));
        }
    }

    return partial_data;
}

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
void PackerVA::PackProcessingInfo(H264DecoderFrameInfo * sliceInfo)
{
    VideoProcessingVA *vpVA = m_va->GetVideoProcessingVA();
    if (!vpVA)
        throw h264_exception(UMC_ERR_FAILED);

    UMCVACompBuffer *pipelineVABuf;
    auto* pipelineBuf = reinterpret_cast<VAProcPipelineParameterBuffer *>(m_va->GetCompBuffer(VAProcPipelineParameterBufferType, &pipelineVABuf, sizeof(VAProcPipelineParameterBuffer)));
    if (!pipelineBuf)
        throw h264_exception(UMC_ERR_FAILED);
    pipelineVABuf->SetDataSize(sizeof(VAProcPipelineParameterBuffer));

    MFX_INTERNAL_CPY(pipelineBuf, &vpVA->m_pipelineParams, sizeof(VAProcPipelineParameterBuffer));

    pipelineBuf->surface = m_va->GetSurfaceID(sliceInfo->m_pFrame->m_index); // should filled in packer
    pipelineBuf->additional_outputs = (VASurfaceID*)vpVA->GetCurrentOutputSurface();
    // To keep output aligned, decode downsampling use this fixed combination of chroma sitting type
    pipelineBuf->input_color_properties.chroma_sample_location = VA_CHROMA_SITING_HORIZONTAL_LEFT | VA_CHROMA_SITING_VERTICAL_CENTER;
}
#endif // #ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE

void PackerVA::PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling)
{
    UMCVACompBuffer *quantBuf;
    auto* pQmatrix_H264 = reinterpret_cast<VAIQMatrixBufferH264 *>(m_va->GetCompBuffer(VAIQMatrixBufferType, &quantBuf, sizeof(VAIQMatrixBufferH264)));
    if (!pQmatrix_H264)
        throw h264_exception(UMC_ERR_FAILED);
    quantBuf->SetDataSize(sizeof(VAIQMatrixBufferH264));

    int32_t j;

    for(j = 0; j < 6; ++j)
    {
        std::copy(std::begin(scaling->ScalingLists4x4[j].ScalingListCoeffs), std::end(scaling->ScalingLists4x4[j].ScalingListCoeffs), std::begin(pQmatrix_H264->ScalingList4x4[j]));
    }

    for(j = 0; j < 2; ++j)
    {
        std::copy(std::begin(scaling->ScalingLists8x8[j].ScalingListCoeffs), std::end(scaling->ScalingLists8x8[j].ScalingListCoeffs), std::begin(pQmatrix_H264->ScalingList8x8[j]));
    }
}

void PackerVA::BeginFrame(H264DecoderFrame* pFrame, int32_t field)
{
    FrameData* fd = pFrame->GetFrameData();
    VM_ASSERT(fd);

    FrameData::FrameAuxInfo* aux;
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    aux = fd->GetAuxInfo(MFX_EXTBUFF_GPU_HANG);
    if (aux)
    {
        VM_ASSERT(aux->type == MFX_EXTBUFF_GPU_HANG);

        mfxExtIntGPUHang* ht = reinterpret_cast<mfxExtIntGPUHang*>(aux->ptr);
        VM_ASSERT(ht && "Buffer pointer should be valid here");
        if (!ht)
            throw h264_exception(UMC_ERR_FAILED);

        //clear trigger to ensure GPU hang fired only once for this frame
        fd->ClearAuxInfo(aux->type);

        UMCVACompBuffer* buffer = NULL;
        m_va->GetCompBuffer(VATriggerCodecHangBufferType, &buffer, sizeof(unsigned int));
        if (buffer)
        {
            unsigned int* trigger =
                reinterpret_cast<unsigned int*>(buffer->GetPtr());
            if (!trigger)
                throw h264_exception(UMC_ERR_FAILED);

            *trigger = 1;
        }
    }
#endif // defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
}

void PackerVA::EndFrame()
{
}

void PackerVA::PackAU(const H264DecoderFrame *pFrame, int32_t isTop)
{
    H264DecoderFrameInfo* sliceInfo =
        const_cast<H264DecoderFrameInfo *>(pFrame->GetAU(isTop));

    uint32_t const count_all = sliceInfo->GetSliceCount();
    if (!m_va || !count_all)
        return;

    uint32_t first_slice = 0;
    H264Slice* slice = sliceInfo->GetSlice(first_slice);

    NAL_Unit_Type const type = slice->GetSliceHeader()->nal_unit_type;
    UMC_H264_DECODER::H264ScalingPicParams const* scaling =
        &slice->GetPicParam()->scaling[type == NAL_UT_CODED_SLICE_EXTENSION ? 1 : 0];
    PackQmatrix(scaling);

    int32_t chopping = CHOPPING_NONE;

    for ( ; first_slice < count_all; )
    {
        PackPicParams(sliceInfo, slice);

        CreateSliceParamBuffer(sliceInfo);
        CreateSliceDataBuffer(sliceInfo);

        uint32_t n = 0, count = 0;
        for (; n < count_all; ++n)
        {
            // put slice header
            H264Slice *pSlice = sliceInfo->GetSlice(first_slice + n);
            chopping = PackSliceParams(pSlice, n, chopping, 0 /* ignored */);
            if (chopping != CHOPPING_SKIP_SLICE)
            {
                ++count;

                if (chopping != CHOPPING_NONE)
                    break;
            }
        }

        first_slice += n;

        UMCVACompBuffer *sliceParamBuf;
        m_va->GetCompBuffer(VASliceParameterBufferType, &sliceParamBuf);
        if (!sliceParamBuf)
            throw h264_exception(UMC_ERR_FAILED);

        sliceParamBuf->SetNumOfItem(count);

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        if (m_va->GetVideoProcessingVA())
            PackProcessingInfo(sliceInfo);
#endif

        Status sts = m_va->Execute();
        if (sts != UMC_OK)
            throw h264_exception(sts);
    }
}

Status PackerVA::QueryStreamOut(H264DecoderFrame* pFrame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "PackerVA::QueryStreamOut");
    return UMC_OK;
}

#endif // UMC_VA_LINUX

} // namespace UMC

#endif // MFX_ENABLE_H264_VIDEO_DECODE
