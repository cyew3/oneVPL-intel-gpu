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

#ifndef __PS_DEC_SETTINGS_H__
#define __PS_DEC_SETTINGS_H__

/* ERR_SIGN */
#define PS_ERR_PARSER               (-13)
#define PS_NO_ERR                   ( 0 )

/* bit stream element */
#define EXTENSION_ID_PS             (0x2)
#define PS_EXTENSION_VER0           ( 0 )

/* MAX MODE */
#define MAX_PS_ENV                  (4+1)
#define MAX_IID_MODE                (5+1)
#define MAX_ICC_MODE                (7+1)
#define MAX_IPD_MODE                (17+1)
#define MAX_OPD_MODE                (MAX_IPD_MODE)

/* resolution bins */
#define NUM_HI_RES                  ( 34 )
#define NUM_MID_RES                 ( 20 )
#define NUM_LOW_RES                 ( 10 )

/* hybrid analysis configuration */
#define CONFIG_HA_BAND34            ( 34 )
#define CONFIG_HA_BAND1020          (1020)

/* quantizations grid for IID */
#define NUM_IID_GRID_STEPS          (7)
#define NUM_IID_FINE_GRID_STEPS     (15)

/* NULL pointer */
#ifndef NULL
#define NULL 0
#endif

/* Hybrid Analysis Filter */
#define LEN_PS_FILTER               (12)
#define DELAY_PS_FILTER             (6)

/* SBR */
#define NUM_SBR_BAND                (32)

/* support of PS */
#define PS_DISABLE                  (0)
#define PS_PARSER                   (1)
#define PS_ENABLE_BL                (111)
#define PS_ENABLE_UR                (411)

/* strategy of mixing L/R channel */
#define PS_MIX_RA                   (0)
#define PS_MIX_RB                   (1)

#endif             /* __PS_DEC_SETTINGS_H__ */
/* EOF */
