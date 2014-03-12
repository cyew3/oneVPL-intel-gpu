/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

class SurfaceIndex;
extern "C" {
void DownSampleMB(SurfaceIndex, SurfaceIndex);
void MeP32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RefineMeP32x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RefineMeP32x16(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RefineMeP16x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RawMeMB_P_Intra(SurfaceIndex, SurfaceIndex, SurfaceIndex , SurfaceIndex);
void RawMeMB_P(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
               SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RawMeMB_B(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void AnalyzeGradient(SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int);
};
