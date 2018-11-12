// Copyright (c) 2014-2018 Intel Corporation
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