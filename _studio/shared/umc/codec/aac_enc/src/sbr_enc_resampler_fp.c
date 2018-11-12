// Copyright (c) 2006-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_ENCODER)

#include "align.h"
#include "sbr_enc_api_fp.h"
#include "aac_status.h"

/********************************************************************/

#ifndef NULL
#define NULL 0
#endif

/********************************************************************/


#define LEN_RS_FILTER 65

static Ipp32f pTabFirCoefsKovalResampler65[] = {
    0.001528047897641f,     0.001041615092844f,     0.000097822857246f,    -0.001299411518046f,
   -0.002317956700839f,    -0.001650433913058f,     0.000601873542942f,     0.003327121906948f,
    0.004171001097190f,     0.001946227878677f,    -0.002725842274920f,    -0.006602610006835f,
   -0.006422148891553f,    -0.000916257798822f,     0.006891796540917f,     0.011535548672663f,
    0.008228313898783f,    -0.002521817453714f,    -0.014394333696814f,    -0.018145919322278f,
   -0.008566583300550f,     0.010775561315008f,     0.027401050935321f,     0.027322985617968f,
    0.004871427658090f,    -0.030146726019331f,    -0.054800191240244f,    -0.044186884614393f,
    0.013276904110205f,     0.106258586046795f,     0.202606266447074f,     0.263760118622812f,
    0.263760118622812f,     0.202606266447074f,     0.106258586046795f,     0.013276904110205f,
   -0.044186884614393f,    -0.054800191240244f,    -0.030146726019331f,     0.004871427658090f,
    0.027322985617968f,     0.027401050935321f,     0.010775561315008f,    -0.008566583300550f,
   -0.018145919322278f,    -0.014394333696814f,    -0.002521817453714f,     0.008228313898783f,
    0.011535548672663f,     0.006891796540917f,    -0.000916257798822f,    -0.006422148891553f,
   -0.006602610006835f,    -0.002725842274920f,     0.001946227878677f,     0.004171001097190f,
    0.003327121906948f,     0.000601873542942f,    -0.001650433913058f,    -0.002317956700839f,
   -0.001299411518046f,     0.000097822857246f,     0.001041615092844f,     0.001528047897641f,
    0.000000000000000f
};
/********************************************************************/

#ifndef NULL
#define NULL 0
#endif

/********************************************************************/

AACStatus sbrencResampler_v2_32f(Ipp32f* pSrc, Ipp32f* pDst)
{
  Ipp32s i, j, k, l;
  Ipp32f sum;
  Ipp32f* pRSCoefTab = pTabFirCoefsKovalResampler65;
  Ipp32s nCoef = LEN_RS_FILTER;

  k = nCoef;
  for (i=0; i<1024; i++ ) {

    sum = 0.0;
    l = k;
    for (j=0; j<nCoef; j++) {
      sum += pRSCoefTab[j] * pSrc[l--];
    }

    pDst[i] = sum;
    k+=2;
  }

  return AAC_OK;
}

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_AAC_AUDIO_ENCODER
