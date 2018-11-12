// Copyright (c) 2011-2018 Intel Corporation
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

#ifndef __SBR_ENC_SETTINGS_H__
#define __SBR_ENC_SETTINGS_H__

#include "sbr_settings.h"

#include "ippac.h"
#include "ipps.h"

/********************************************************************/
/*          SBR ENC SUPPORT FREQ                                    */
/********************************************************************/

#define MIN_SBR_ENC_FREQ                    16000

#define MAX_SBR_ENC_FREQ                    96000

/********************************************************************/
/*                                                                  */
/********************************************************************/

#define MAX_NUM_NOISE_BANDS                    18
#define MAX_NUM_NOISE_VALUES                   10

#define MAX_NUM_FREQ_COEFFS                    27

#define MAX_NUM_REL                             3

#define INVF_SMOOTH_LEN                         2
#define NF_SMOOTHING_LENGTH                     4

/* TYPE DOMAIN CODING */
#define FREQ_DOMAIN                             0
#define TIME_DOMAIN                             1

/********************************************************************/

#define SBR_MAX_SIZE_PAYLOAD_BITS            270*8

/********************************************************************/

#define SBR_L      0

#define SBR_R      1

/********************************************************************/

//#define SIZE_FRAME_AAC 1024

/********************************************************************/

/* see documentations 3GPP TS 26.404 */
/* buffer_state of analysis (640) is hidden */

/* TOT_CORE_AAC_DELAY = 512(MDCT ENC) + 512(IMDCT DEC) + 1024(PSY ENC) */
#define TOT_CORE_AAC_DELAY    2048 //1600 - gpp //(2048) - umc

/* TIME_INPUT_DELAY = TOT_CORE_AAC_DELAY*2 + SBR_DEC_DELAY - SBR_ENC_DELAY */
#define TIME_INPUT_DELAY      (TOT_CORE_AAC_DELAY*2 + 6*64 - 32*64 ) //+ 1) // +1 only tmp from 3gpp synchr

#define SBR_SD_FILTER_DELAY   32 //6 -3gpp synchr  //(32) - real umc

#define TOT_SBR_INPUT_DELAY   (TIME_INPUT_DELAY + SBR_SD_FILTER_DELAY)

#define SBR_QMFA_DELAY     576

#define SIZE_OF_SBR_INPUT_BUF (SBR_QMFA_DELAY + 2048 + TOT_SBR_INPUT_DELAY)

/* sbr header is sent every 500 ms */
#define SBR_HEADER_SEND_TIME 500

#ifndef EXT_SBR_DATA
#define EXT_SBR_DATA        0x0D
#endif

#ifndef EXT_SBR_DATA_CRC
#define EXT_SBR_DATA_CRC    0x0E
#endif

/********************************************************************/
/*                     PLATFORM DEPEND CONST                        */
/********************************************************************/

#define EPS                     1e-18f
#define LOG2                    0.69314718056f
#define ILOG2                   1.442695041f
#define RELAXATION              1e-6f

/********************************************************************/
/*                    SINES ESTIMATION ALGORITHM                    */
/********************************************************************/

#define DELTA_TIME              9
#define MAX_COMP                2
#define TONALITY_QUOTA          0.1f
#define DIFF_QUOTA              0.75f
#define THR_DIFF               25.0f
#define THR_DIFF_GUIDE          1.26f
#define THR_TONE               15.0f
#define INV_THR_TONE          (1.0f/15.0f)
#define THR_TONE_GUIDE          1.26f
#define THR_SFM_SBR             0.3f
#define THR_SFM_ORIG            0.1f
#define DECAY_GUIDE_ORIG        0.3f
#define DECAY_GUIDE_DIFF        0.5f

/********************************************************************/

#endif             /* __SBR_ENC_SETTINGS_H__ */
