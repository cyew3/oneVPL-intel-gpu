//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef MODEL_H
#define MODEL_H

#include "../common.h"

unsigned int classify(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif,
                                                      double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture,
                                                      double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB);

unsigned int classify_f1(double dTextureLevel, double dRsG, double dStatDif, double dBigTexture, double dSADv);
unsigned int classify_f2(double dRsG, double dDynDif, double dAngle, double dSADv, double dZeroTexture);
unsigned int classify_f3(double dTextureLevel, double dRsG, double dCountDif, double dRsT, double dCount);
unsigned int classify_f4(double dTextureLevel, double dStatDif, double dZeroTexture, double dStatCount, double dSADv);
unsigned int classify_f5(double dTextureLevel, double dDynDif, double dCountDif, double dStatCount, double dStatDif);
unsigned int classify_f6(double dTextureLevel, double dRsG, double dRsT, double dStatDif, double dCount);
unsigned int classify_f7(double dTextureLevel, double dDynDif, double dRsDif, double dStatCount, double dZeroTexture);
unsigned int classify_f8(double dDynDif, double dAngle, double SADCTPB, double dRsB, double dRsDif);
unsigned int classify_f9(double SADCBPT, double dAngle, double dRsT, double SADCTPB, double dCount);
unsigned int classify_fA(double dRsG, double dStatDif, double dBigTexture, double dStatCount, double dSADv);
unsigned int classify_fB(double dRsT, double dCountDif, double dStatDif, double SADCTPB, double dZeroTexture);

unsigned int classify_fC(double dRsG, double dBigTexture, double dAngle, double SADCBPT, double dZeroTexture);
unsigned int classify_fD(double dTextureLevel, double dSADv, double dStatDif, double dStatCount, double dZeroTexture);
unsigned int classify_fE(double dRsT, double dCountDif, double SADCBPT, double dAngle, double dRsDif);
unsigned int classify_fF(double dRsT, double dStatDif, double SADCTPB, double dRsB, double dCount);
unsigned int classify_f10(double dRsG, double dDynDif, double dCountDif, double dRsB, double dStatDif);
unsigned int classify_f11(double dRsG, double dDynDif, double dAngle, double dStatDif, double dCount);
#endif