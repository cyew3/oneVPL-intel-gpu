// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __SBR_DEC_SETTINGS_H__
#define __SBR_DEC_SETTINGS_H__

/* my val */
#define SBR_NOT_SCALABLE   0
#define SBR_MONO_BASE      1
#define SBR_STEREO_ENHANCE 2
#define SBR_STEREO_BASE    3
#define SBR_NO_DATA        4

/* standard */
#define NUM_TIME_SLOTS     16
#define SBR_DEQUANT_OFFSET 6
#define RATE               2

#define SBR_TIME_HFGEN     8
#define SBR_TIME_HFADJ     2

/* user params for decoder */
#define HEAAC_HQ_MODE      18
#define HEAAC_LP_MODE      28

#define HEAAC_DWNSMPL_ON   1
#define HEAAC_DWNSMPL_OFF  0

#define HEAAC_PARAMS_UNDEF -999

/* SIZE OF WORK BUFFER */
#define SBR_MINSIZE_OF_WORK_BUFFER 6 * 100 * sizeof(Ipp32s)

//#define MAX_NUM_ENV_VAL  100

//#define MAX_NUM_NOISE_VAL  5

/********************************************************************/

/* ENABLE SBR */
typedef enum {
  SBR_DISABLE = 0,
  SBR_ENABLE  = 777,
  SBR_UNDEF   = -999

}eSBR_SUPPORT;

#endif             /* __SBR_DEC_SETTINGS_H__ */
