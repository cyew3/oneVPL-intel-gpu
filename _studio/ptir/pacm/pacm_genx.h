//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_media_deinterlace_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstTopFieldSurfaceId, SurfaceIndex dstBotFieldSurfaceId);

template<bool IS_TOP_FIELD, unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_media_deinterlace_single_field_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFieldSurfaceId);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT, unsigned int TOP_FIELD>
void cmk_undo2frame_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFrameSurfaceId);

template <unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT, unsigned int TOP_FIELD>
void cmk_DeinterlaceBorder(SurfaceIndex inputSurface, unsigned int WIDTH, unsigned int HEIGHT);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_rs_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height);

void cmk_FixEdgeDirectionalIYUV_Main_Bottom_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC);
void cmk_FixEdgeDirectionalIYUV_Main_Top_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_rs_nv12(SurfaceIndex frameSurfaceId, SurfaceIndex rsSurfaceId, int Height);

template <unsigned int WIDTH, int BotBase, unsigned int THREADYSCALE>
void  cmk_FilterMask_Main(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height);

template <unsigned int MAXWIDTH, int BotBase, unsigned int THREADYSCALE>
void cmk_FilterMask_Main_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);

template <unsigned int WIDTH, unsigned int THREADYSCALE>
void cmk_FilterMask_Main_2Fields(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height);

template <unsigned int THREADYSCALE>
void cmk_FilterMask_Main_2Fields_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_rs_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height);

