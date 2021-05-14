// Copyright (c) 2014-2019 Intel Corporation
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


#pragma once

#include "mfx_av1_defs.h"

#ifdef _DEBUG
#define AV1_BITCOUNT
#endif

//#define AV1_TRACE_CABAC
//#define AV1_TRACE_COST
//#define Av1_TRACE_PROB_UPDATE

namespace AV1Enc {

    extern int32_t traceCabac;
    extern int32_t symbCounter;
    extern int32_t bitCounter;

    inline void EnableTraceCabac() { traceCabac=1; }
    inline void DisableTraceCabac() { traceCabac=0; }


#ifdef AV1_TRACE_CABAC
    inline void fprintf_trace_cabac(FILE *file, const char *format, ...) {
        if (traceCabac) {
            va_list args;
            va_start(args, format);
            vfprintf(file, format, args);
            va_end(args);
        }
    }

    inline void trace_cdf_no_update(int s, const unsigned short *cdf, int nsyms, int range) {
        if (traceCabac) {
            fprintf_trace_cabac(stderr, "%d: symb=%d rng=%d cdf=[ ", symbCounter++, s, range);
            for (int i = 0; i < nsyms; i++)
                fprintf_trace_cabac(stderr, "%d ", cdf[i]);
            fprintf_trace_cabac(stderr, "]\n");
        }
    }

    inline void trace_cdf(int s, const unsigned short *cdf, int nsyms, int range) {
        if (traceCabac) {
            fprintf_trace_cabac(stderr, "%d: symb=%d rng=%d cdf=[ ", symbCounter++, s, range);
            for (int i = 0; i < nsyms + 1; i++)
                fprintf_trace_cabac(stderr, "%d ", cdf[i]);
            fprintf_trace_cabac(stderr, "] ");
        }
    }

    inline void trace_cdf_update(const unsigned short *cdf, int nsyms) {
        fprintf_trace_cabac(stderr, "upd=[ ");
        for (int i = 0; i < nsyms + 1; i++)
            fprintf_trace_cabac(stderr, "%d ", cdf[i]);
        fprintf_trace_cabac(stderr, "]\n");
    }

    inline void trace_bool(int bit, int p, int range) {
        if (traceCabac)
            fprintf_trace_cabac(stderr, "%d: bool=%d rng=%d prob=%d\n", symbCounter++, bit, range, p);
    }

    inline void trace_putbit(int bit) {
        fprintf_trace_cabac(stderr, "%d: bit=%d\n", bitCounter++, bit);
    }

    inline void trace_motion_vector(const AV1MVPair &mv) {
        fprintf_trace_cabac(stderr, "mv0=(%d,%d) mv1=(%d,%d)\n", mv[0].mvx, mv[0].mvy, mv[1].mvx, mv[1].mvy);
    }

#else // AV1_TRACE_CABAC
    inline void fprintf_trace_cabac(FILE *, const char *, ...) {}
    inline void trace_cdf_no_update(int s, const unsigned short *, int, int) {}
    inline void trace_cdf(int, const unsigned short *, int, int) {}
    inline void trace_cdf_update(const unsigned short *, int) {}
    inline void trace_bool(int, int, int) {}
    inline void trace_motion_vector(const AV1MVPair &) {}
#endif // AV1_TRACE_CABAC


#ifdef AV1_TRACE_COST
    inline void fprintf_trace_cost(FILE *file, const char *format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
    }
#else
    inline void fprintf_trace_cost(FILE *, const char *, ...) {}
#endif


#ifdef AV1_TRACE_PROB_UPDATE
    inline void fprintf_trace_prob_update(FILE *file, const char *format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
    }
#else
    inline void fprintf_trace_prob_update(FILE *, const char *, ...) {}
#endif

}  // namespace AV1Enc