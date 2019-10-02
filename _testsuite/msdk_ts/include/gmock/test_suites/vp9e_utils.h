/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

// set typical segmentation params for sanity checks
inline bool SetDefaultSegmentationParams(mfxExtVP9Segmentation &segmentation_ext_params, const mfxVideoParam& par, HWType platform)
{
    if (!segmentation_ext_params.NumSegments)
    {
        segmentation_ext_params.NumSegments = 3;
    }

    if (!segmentation_ext_params.SegmentIdBlockSize)
    {
        if (platform < MFX_HW_DG2)
            segmentation_ext_params.SegmentIdBlockSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_64x64;
        else
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

inline mfxU32 ARTIFACT_DETECTOR(tsFrame& ref, tsFrame& src, mfxU32 id)
{
    mfxF64 size = ref.m_info.CropW * ref.m_info.CropH;
    mfxI32 diff = 0;
    mfxU32 artifacts_1edge = 0;
    mfxU32 artifacts_2edges = 0;
    mfxU32 chroma_step = 1;
    mfxU32 maxw = TS_MIN(ref.m_info.CropW, src.m_info.CropW);
    mfxU32 maxh = TS_MIN(ref.m_info.CropH, src.m_info.CropH);
    mfxU16 bd0 = id ? TS_MAX(8, ref.m_info.BitDepthChroma) : TS_MAX(8, ref.m_info.BitDepthLuma);
    mfxU16 bd1 = id ? TS_MAX(8, src.m_info.BitDepthChroma) : TS_MAX(8, src.m_info.BitDepthLuma);
    mfxF64 max = (1 << bd0) - 1;

    if (bd0 != bd1)
        g_tsStatus.check(MFX_ERR_UNSUPPORTED);

    if (0 != id)
    {
        if (ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400
            || src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
        {
            g_tsStatus.check(MFX_ERR_UNSUPPORTED);
            return 0;
        }
        if (ref.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420
            && src.m_info.ChromaFormat == MFX_CHROMAFORMAT_YUV420)
        {
            chroma_step = 2;
            size /= 4;
        }
    }

#define COUNT_ARTIFATCS(COMPONENT, STEP, THRESHOLD, LOG) \
        for(mfxU32 y = 0; y < maxh; y += STEP) \
        { \
            for(mfxU32 x = 0; x < maxw; x += STEP) \
            { \
                int atype = 0; \
                int v = ref.COMPONENT(x + src.m_info.CropX, y + src.m_info.CropY); \
                int v1 = src.COMPONENT(x + src.m_info.CropX, y + src.m_info.CropY); \
                int v2 = 0; \
                int aresult = abs(v1 - v); \
                if (aresult > THRESHOLD) \
                { \
                    mfxU32 artifact_found = 0; \
                    if (x) \
                    { \
                        v2 = src.COMPONENT((x - STEP) + src.m_info.CropX, y + src.m_info.CropY); \
                        if(abs(v1 - v2) > THRESHOLD) \
                        { \
                            artifact_found++; \
                            atype = 1; \
                            if(LOG) printf("ARTIFACT T1 [%dx%d-%d] VALUE=%d diff_EXT=%d diff_INT=%d TR=%d \n", x, y, id, v1, aresult, abs(v1 - v2), THRESHOLD); fflush(0); \
                        } \
                    } \
                    if (y) \
                    { \
                        v2 = src.COMPONENT(x + src.m_info.CropX, (y - STEP) + src.m_info.CropY); \
                        if (abs(v1 - v2) > THRESHOLD) \
                        { \
                            artifact_found++; \
                            atype = 2; \
                            if(LOG) printf("ARTIFACT T2 [%dx%d-%d] VALUE=%d diff_EXT=%d diff_INT=%d TR=%d \n", x, y, id, v1, aresult, abs(v1 - v2), THRESHOLD); fflush(0); \
                        } \
                    } \
                    if (x < (maxw - 1)) \
                    { \
                        v2 = src.COMPONENT((x + STEP) + src.m_info.CropX, y + src.m_info.CropY); \
                        if (abs(v1 - v2) > THRESHOLD) \
                        { \
                            artifact_found++; \
                            atype = 3; \
                            if(LOG) printf("ARTIFACT T3 [%dx%d-%d] VALUE=%d diff_EXT=%d diff_INT=%d TR=%d \n", x, y, id, v1, aresult, abs(v1 - v2), THRESHOLD); fflush(0); \
                        } \
                    } \
                    if (y < (maxh)) \
                    { \
                        v2 = src.COMPONENT(x + src.m_info.CropX, (y + STEP) + src.m_info.CropY); \
                        if (abs(v1 - v2) > THRESHOLD) \
                        { \
                            artifact_found++; \
                            atype = 4; \
                            if(LOG) printf("ARTIFACT T4 [%dx%d-%d] VALUE=%d diff_EXT=%d diff_INT=%d TR=%d \n", x, y, id, v1, aresult, abs(v1 - v2), THRESHOLD); fflush(0); \
                        } \
                    } \
                    if(artifact_found > 1) \
                    { \
                        artifacts_2edges++; \
                    } \
                    else if(artifact_found > 0) \
                    { \
                        artifacts_1edge++; \
                    } \
                } \
            } \
        }

    /*threshold for pixels difference, 8bit*/
#define DIFF_THRESHOLD (40)

    bool enable_log = false;
#ifdef ENABLE_ARTIFACTS_LOG
    enable_log = true;
#endif

    if (ref.isYUV() && src.isYUV())
    {
        switch (id)
        {
        case 0:  COUNT_ARTIFATCS(Y, 1, DIFF_THRESHOLD, enable_log); break;
        case 1:  COUNT_ARTIFATCS(U, chroma_step, DIFF_THRESHOLD, enable_log); break;
        case 2:  COUNT_ARTIFATCS(V, chroma_step, DIFF_THRESHOLD, enable_log); break;
        default: g_tsStatus.check(MFX_ERR_UNSUPPORTED); break;
        }
    }
    else if (ref.isYUV16() && src.isYUV16())
    {
        switch (id)
        {
            /*for REXT x3 of the basic threshold*/
        case 0:  COUNT_ARTIFATCS(Y16, 1, DIFF_THRESHOLD * 3, enable_log); break;
        case 1:  COUNT_ARTIFATCS(U16, chroma_step, DIFF_THRESHOLD * 3, enable_log); break;
        case 2:  COUNT_ARTIFATCS(V16, chroma_step, DIFF_THRESHOLD * 3, enable_log); break;
        default: g_tsStatus.check(MFX_ERR_UNSUPPORTED); break;
        }
    }
    else
        g_tsStatus.check(MFX_ERR_UNSUPPORTED);

    if (artifacts_2edges > 5 /*threshold for pixels that differ much from 2 neighbors*/)
    {
        return (artifacts_2edges * 2);
    }
    else if (artifacts_1edge > (20 / chroma_step) /*threshold for pixels that differ much from 1 neighbor*/)
    {
        return artifacts_1edge;
    }
    else
    {
        return 0;
    }
}