/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef CMRT_EMU

#include <cm_common.h>

extern "C" {
void DownSampleMB(SurfaceIndex, SurfaceIndex) {}
void MeP32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void RefineMeP32x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void RefineMeP32x16(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void RefineMeP16x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void RawMeMB_P_Intra(SurfaceIndex, SurfaceIndex, SurfaceIndex , SurfaceIndex ) {}
void RawMeMB_P(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
               SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void RawMeMB_B(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex) {}
void AnalyzeGradient(SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int) {}
void InterpolateFrame(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                      SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG) {}
};

#endif // !CMRT_EMU
