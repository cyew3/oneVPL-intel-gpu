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
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_mfx_sw_supplier.h"
#include "umc_h264_task_broker_mt.h"

namespace UMC
{
void MFX_SW_TaskSupplier::CreateTaskBroker()
{
    switch(m_iThreadNum)
    {
    case 1:
        m_pTaskBroker = new TaskBrokerSingleThread(this);
        break;
    case 4:
    case 3:
    case 2:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    default:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    };

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264SegmentDecoderMultiThreaded(m_pTaskBroker);
    }
}

void MFX_SW_TaskSupplier::SetMBMap(const H264Slice * slice, H264DecoderFrame *frame, LocalResources * localRes)
{
    if (!slice)
        slice = frame->GetAU(0)->GetSlice(0);

    const H264PicParamSet *pps = slice->GetPicParam();
    VM_ASSERT(frame->m_iResourceNumber >= 0);

    Ipp32s resource = frame->m_iResourceNumber;

    if (pps->num_slice_groups == 1)
    {
        localRes->GetMBInfo(resource).active_next_mb_table = localRes->GetDefaultMBMapTable();
        return;
    }

    Ipp32s additionalTable = resource + 1;
    const H264SeqParamSet *sps = slice->GetSeqParam();
    const H264SliceHeader * sliceHeader = slice->GetSliceHeader();

    Ipp32u PicWidthInMbs = slice->GetSeqParam()->frame_width_in_mbs;
    Ipp32u PicHeightInMapUnits = slice->GetSeqParam()->frame_height_in_mbs;
    if (sliceHeader->field_pic_flag)
        PicHeightInMapUnits >>= 1;

    Ipp32u PicSizeInMbs = PicWidthInMbs*PicHeightInMapUnits;

    Ipp32s *mapUnitToSliceGroupMap = new Ipp32s[PicSizeInMbs];
    Ipp32s *MbToSliceGroupMap = new Ipp32s[PicSizeInMbs];

    for (Ipp32u i = 0; i < PicSizeInMbs; i++)
        mapUnitToSliceGroupMap[i] = 0;

    switch (pps->SliceGroupInfo.slice_group_map_type)
    {
        case 0:
        {
            // interleaved slice groups: run_length for each slice group,
            // repeated until all MB's are assigned to a slice group
            Ipp32u i = 0;
            do
                for(Ipp32u iGroup = 0; iGroup < pps->num_slice_groups && i < PicSizeInMbs; i += pps->SliceGroupInfo.run_length[iGroup++])
                    for(Ipp32u j = 0; j < pps->SliceGroupInfo.run_length[iGroup] && i + j < PicSizeInMbs; j++)
                        mapUnitToSliceGroupMap[i + j] = iGroup;
            while(i < PicSizeInMbs);
        }
        break;

    case 1:
        {
            // dispersed
            for(Ipp32u i = 0; i < PicSizeInMbs; i++ )
                mapUnitToSliceGroupMap[i] = (((i % PicWidthInMbs) + (((i / PicWidthInMbs) * pps->num_slice_groups) / 2)) % pps->num_slice_groups);
        }
        break;

    case 2:
        {
            // foreground + leftover: Slice groups are rectangles, any MB not
            // in a defined rectangle is in the leftover slice group, a MB within
            // more than one rectangle is in the lower-numbered slice group.

            for(Ipp32u i = 0; i < PicSizeInMbs; i++)
                mapUnitToSliceGroupMap[i] = (pps->num_slice_groups - 1);

            for(Ipp32s iGroup = pps->num_slice_groups - 2; iGroup >= 0; iGroup--)
            {
                Ipp32u yTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] / PicWidthInMbs;
                Ipp32u xTopLeft = pps->SliceGroupInfo.t1.top_left[iGroup] % PicWidthInMbs;
                Ipp32u yBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] / PicWidthInMbs;
                Ipp32u xBottomRight = pps->SliceGroupInfo.t1.bottom_right[iGroup] % PicWidthInMbs;

                for(Ipp32u y = yTopLeft; y <= yBottomRight; y++)
                    for(Ipp32u x = xTopLeft; x <= xBottomRight; x++)
                        mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = iGroup;
            }
        }
        break;

    case 3:
        {
            // Box-out, clockwise or counterclockwise. Result is two slice groups,
            // group 0 included by the box, group 1 excluded.

            Ipp32s x, y, leftBound, rightBound, topBound, bottomBound;
            Ipp32s mapUnitVacant = 0;
            Ipp8s xDir, yDir;
            Ipp8u dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;

            x = leftBound = rightBound = (PicWidthInMbs - dir_flag) / 2;
            y = topBound = bottomBound = (PicHeightInMapUnits - dir_flag) / 2;

            xDir  = dir_flag - 1;
            yDir  = dir_flag;

            Ipp32u uNumInGroup0 = IPP_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);

            for(Ipp32u i = 0; i < PicSizeInMbs; i++)
                mapUnitToSliceGroupMap[i] = 1;

            for(Ipp32u k = 0; k < uNumInGroup0; k += mapUnitVacant)
            {
                mapUnitVacant = (mapUnitToSliceGroupMap[y * PicWidthInMbs + x] == 1);

                if(mapUnitVacant)
                    mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = 0;

                if(xDir == -1 && x == leftBound)
                {
                    leftBound = IPP_MAX(leftBound - 1, 0);
                    x = leftBound;
                    xDir = 0;
                    yDir = 2 * dir_flag - 1;
                }
                else if(xDir == 1 && x == rightBound)
                {
                    rightBound = IPP_MIN(rightBound + 1, (Ipp32s)PicWidthInMbs - 1);
                    x = rightBound;
                    xDir = 0;
                    yDir = 1 - 2 * dir_flag;
                }
                else if(yDir == -1 && y == topBound)
                {
                    topBound = IPP_MAX(topBound - 1, 0);
                    y = topBound;
                    xDir = 1 - 2 * dir_flag;
                    yDir = 0;
                }
                else if(yDir == 1 && y == bottomBound)
                {
                    bottomBound = IPP_MIN(bottomBound + 1, (Ipp32s)PicHeightInMapUnits - 1);
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
            Ipp32u uNumInGroup0 = IPP_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);
            Ipp8u dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            Ipp32u sizeOfUpperLeftGroup = (dir_flag ? (PicSizeInMbs - uNumInGroup0) : uNumInGroup0);

            for(Ipp32u i = 0; i < PicSizeInMbs; i++)
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

            Ipp32u uNumInGroup0 = IPP_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate * sliceHeader->slice_group_change_cycle, PicSizeInMbs);
            Ipp8u dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
            Ipp32u sizeOfUpperLeftGroup = (dir_flag ? (PicSizeInMbs - uNumInGroup0) : uNumInGroup0);

            Ipp32u k = 0;
            for(Ipp32u j = 0; j < PicWidthInMbs; j++)
                for(Ipp32u i = 0; i < PicHeightInMapUnits; i++)
                    if(k++ < sizeOfUpperLeftGroup)
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = dir_flag;
                    else
                        mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = 1 - dir_flag;
        }
        break;

    case 6:
        {
            // explicit map read from bitstream, contains slice group id for
            // each map unit
            for(Ipp32u i = 0; i < pps->SliceGroupInfo.pSliceGroupIDMap.size(); i++)
                mapUnitToSliceGroupMap[i] = pps->SliceGroupInfo.pSliceGroupIDMap[i];
        }
        break;

    default:
        // can't happen
        break;
    }    // switch map type

    // Filling array groupMap as in H264 standart

    if (sps->frame_mbs_only_flag || sliceHeader->field_pic_flag)
    {
        for(Ipp32u i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[i];
        }
    }
    else if (sliceHeader->MbaffFrameFlag)
    {
        for(Ipp32u i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[i/2];
        }
    }
    else if(sps->frame_mbs_only_flag == 0 && !sliceHeader->MbaffFrameFlag && !sliceHeader->field_pic_flag)
    {
        for(Ipp32u i = 0; i < PicSizeInMbs; i++ )
        {
            MbToSliceGroupMap[i] = mapUnitToSliceGroupMap[(i / (2 * PicWidthInMbs)) * PicWidthInMbs + ( i % PicWidthInMbs )];
        }
    }

    H264DecoderMBAddr * next_mb_tables = localRes->next_mb_tables[additionalTable];

    for(Ipp32u n = 0; n < PicSizeInMbs; n++)
    {
        Ipp32u i = n + 1;
        while (i < PicSizeInMbs && MbToSliceGroupMap[i] != MbToSliceGroupMap[n])
            i++;

        next_mb_tables[n] = (i >= PicSizeInMbs) ? -1 : i;
    }

    if (sliceHeader->field_pic_flag)
    {
        for(Ipp32u i = 0; i < PicSizeInMbs; i++ )
        {
            next_mb_tables[i + PicSizeInMbs] = (next_mb_tables[i] == -1) ? -1 : (next_mb_tables[i] + PicSizeInMbs);
        }
    }

    localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];

    delete[] mapUnitToSliceGroupMap;
    delete[] MbToSliceGroupMap;
}

}
#endif // UMC_ENABLE_H264_VIDEO_DECODER
