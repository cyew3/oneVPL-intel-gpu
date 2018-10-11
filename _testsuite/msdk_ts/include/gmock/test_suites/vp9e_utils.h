/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

// set typical segmentation params for sanity checks
inline bool SetDefaultSegmentationParams(mfxExtVP9Segmentation &segmentation_ext_params, const mfxVideoParam& par)
{
    if (!segmentation_ext_params.NumSegments)
    {
        segmentation_ext_params.NumSegments = 3;
    }

    if (!segmentation_ext_params.SegmentIdBlockSize)
    {
        segmentation_ext_params.SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32;
    }

    for (mfxU32 i = 0; i < segmentation_ext_params.NumSegments; ++i)
    {
        segmentation_ext_params.Segment[i].QIndexDelta = ((i % 2) == 1 ? -1 : 1) * 10 * i;
    }

    mfxU32 seg_block = 8;
    if (segmentation_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16)
    {
        seg_block = 16;
    }
    else if (segmentation_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32)
    {
        seg_block = 32;
    }
    else if (segmentation_ext_params.SegmentIdBlockSize == MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64)
    {
        seg_block = 64;
    }

    for (mfxU32 i = 0; i < segmentation_ext_params.NumSegments; ++i)
    {
        if (segmentation_ext_params.Segment[i].QIndexDelta)
            segmentation_ext_params.Segment[i].FeatureEnabled = MFX_VP9_SEGMENT_FEATURE_QINDEX;
        if (segmentation_ext_params.Segment[i].LoopFilterLevelDelta)
            segmentation_ext_params.Segment[i].FeatureEnabled |= MFX_VP9_SEGMENT_FEATURE_LOOP_FILTER;
        if (segmentation_ext_params.Segment[i].ReferenceFrame)
            segmentation_ext_params.Segment[i].FeatureEnabled |= MFX_VP9_SEGMENT_FEATURE_REFERENCE;
    }

    const mfxU32 map_width_qnt = (par.mfx.FrameInfo.Width + (seg_block - 1)) / seg_block;
    const mfxU32 map_height_qnt = (par.mfx.FrameInfo.Height + (seg_block - 1)) / seg_block;

    segmentation_ext_params.NumSegmentIdAlloc = map_width_qnt * map_height_qnt;

    // NB: don't forget to release this ptr or memory leak
    segmentation_ext_params.SegmentId = new mfxU8[segmentation_ext_params.NumSegmentIdAlloc];
    memset(segmentation_ext_params.SegmentId, 0, segmentation_ext_params.NumSegmentIdAlloc);

    for (mfxU32 i = 0; i < segmentation_ext_params.NumSegmentIdAlloc; ++i)
    {
        segmentation_ext_params.SegmentId[i] = i % segmentation_ext_params.NumSegments;
    }

    return true;
}