/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: model.h

\* ****************************************************************************** */

#ifndef MODEL_H
#define MODEL_H

__declspec(dllexport) unsigned int __stdcall classify(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif,
	                                                  double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture,
	                                                  double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB);


#endif