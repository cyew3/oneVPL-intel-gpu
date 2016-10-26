//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __OUTLINE_WRITER_DEFS_H__
#define __OUTLINE_WRITER_DEFS_H__

#define ID                  VM_STRING("id")

#define SEQ_SEQUENCE            VM_STRING("sequence")
#define SEQ_COLOR_FORMAT        VM_STRING("color_format")
#define SEQ_FRAME_WIDTH         VM_STRING("width")
#define SEQ_FRAME_HEIGHT        VM_STRING("height")
#define SEQ_FRAME_CROP_WIDTH    VM_STRING("crop_width")
#define SEQ_FRAME_CROP_HEIGHT   VM_STRING("crop_height")
#define SEQ_FRAME_CROP_X        VM_STRING("crop_x")
#define SEQ_FRAME_CROP_Y        VM_STRING("crop_y")

#define SEQ_HORZ_ASPECT         VM_STRING("horz_aspect")
#define SEQ_VERT_ASPECT         VM_STRING("vert_aspect")

#define SEQ_PROFILE             VM_STRING("profile")
#define SEQ_LEVEL               VM_STRING("level")
#define SEQ_FRAME_RATE          VM_STRING("frame_rate")
#define SEQ_BITDEPTH_LUMA       VM_STRING("bitdepth_luma")
#define SEQ_BITDEPTH_CHROMA     VM_STRING("bitdepth_chroma")
#define SEQ_BITDEPTH_ALPHA      VM_STRING("bitdepth_alpha")
#define SEQ_ALPHA_CHANNEL_PRESENT    VM_STRING("is_alpha_present")


#define FRAME               VM_STRING("f")
#define FRAME_TYPE          VM_STRING("t")
#define TIME_STAMP          VM_STRING("ts")
#define PICTURE_STRUCTURE   VM_STRING("ps")
#define CRC32               VM_STRING("crc")
#define IS_INVALID          VM_STRING("error")

#define MFX_PICTURE_STRUCTURE   VM_STRING("m_ps")
#define MFX_DATA_FLAG           VM_STRING("m_data")
#define MFX_VIEW_ID             VM_STRING("m_view")
#define MFX_TEMPORAL_ID         VM_STRING("m_temporal")
#define MFX_PRIORITY_ID         VM_STRING("m_priority")

#define PLANES_INFO         VM_STRING("pi")

#define PLANE               VM_STRING("p")
#define NUM                 VM_STRING("n")
#define WIDTH               VM_STRING("w")
#define HEIGHT              VM_STRING("h")
#define NUM_SAMPLES         VM_STRING("ns")
#define SAMPLE_SIZE         VM_STRING("ss")
#define BIT_DEPTH           VM_STRING("bd")

#define XML_HEADER          VM_STRING("<?xml version=\"1.0\" encoding=\"utf-8\" ?>")
#define DTD_HEADER          VM_STRING("<!DOCTYPE outline [<!ATTLIST f id ID #REQUIRED>]>")
#define OUTLINE             VM_STRING("outline")


#endif // #ifndef __OUTLINE_WRITER_DEFS_H__
