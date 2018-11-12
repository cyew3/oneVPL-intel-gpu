// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_DEBUG_H__
#define __UMC_VC1_DEC_DEBUG_H__

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "umc_vc1_dec_seq.h"
#include "vm_types.h"



    extern const Ipp32u  VC1_POSITION; // MB, Block positions, skip info
    extern const Ipp32u  VC1_CBP; // coded block patern info
    extern const Ipp32u  VC1_BITBLANES; // bitplane information
    extern const Ipp32u  VC1_QUANT; // transform types decoded info
    extern const Ipp32u  VC1_TT; // transform types decoded info
    extern const Ipp32u  VC1_MV; // motion vectors info
    extern const Ipp32u  VC1_PRED; // predicted blocks
    extern const Ipp32u  VC1_COEFFS; // DC, AC coefficiens
    extern const Ipp32u  VC1_RESPEL; // pixels befor filtering
    extern const Ipp32u  VC1_SMOOTHINT; // smoothing
    extern const Ipp32u  VC1_BFRAMES; // B frames log
    extern const Ipp32u  VC1_INTENS; // intesity compensation tables
    extern const Ipp32u  VC1_MV_BBL; // deblocking
    extern const Ipp32u  VC1_MV_FIELD; // motion vectors info for field pic

    extern const Ipp32u  VC1_DEBUG; //current debug output
    extern const Ipp32u  VC1_FRAME_DEBUG; //on/off frame debug
    extern const Ipp32u  VC1_FRAME_MIN; //first frame to debug
    extern const Ipp32u  VC1_FRAME_MAX; //last frame to debug
    extern const Ipp32u  VC1_TABLES; //VLC tables

    typedef enum
    {
        VC1DebugAlloc,
        VC1DebugFree,
        VC1DebugRoutine
    } VC1DebugWork;

class VM_Debug
{
public:

    void vm_debug_frame(Ipp32s _cur_frame, Ipp32s level, const vm_char *format, ...);

#ifdef ALLOW_SW_VC1_FALLBACK
    void _print_macroblocks(VC1Context* pContext);
    void _print_blocks(VC1Context* pContext);
#endif
    void print_bitplane(VC1Bitplane* pBitplane, Ipp32s width, Ipp32s height);
    static VM_Debug& GetInstance(VC1DebugWork typeWork)
    {
        static VM_Debug* singleton;
        switch (typeWork)
        {
        case VC1DebugAlloc:
            singleton = new VM_Debug;
            break;
        case VC1DebugRoutine:
            break;
        default:
            break;
        }
        return *singleton;
    }

    void Release()
    {
        delete this;
    }
#if defined (_WIN32) && (_DEBUG)
    VM_Debug():DebugThreadID(GetCurrentThreadId())
#else
    VM_Debug()
#endif
    {
#ifdef VC1_DEBUG_ON
        Logthread0 = vm_file_fopen(VM_STRING("_Log0.txt"),VM_STRING("w"));
        Logthread1 = vm_file_fopen(VM_STRING("_Log1.txt"),VM_STRING("w"));

        VM_ASSERT(Logthread0 != NULL);
        VM_ASSERT(Logthread1 != NULL);
#endif

    }; //only for Win debug

    ~VM_Debug()
    {
#ifdef VC1_DEBUG_ON
        vm_file_fclose(Logthread0);
        Logthread0 = NULL;
        vm_file_fclose(Logthread1);
        Logthread1 = NULL;
#endif
    };
#if defined (_WIN32) && (_DEBUG)
    void setThreadToDebug(Ipp32u threadID)
    {
        DebugThreadID = threadID;
    }
#endif

private:
#if defined (_WIN32) && (_DEBUG)
    Ipp32u DebugThreadID;
#endif

#ifdef VC1_DEBUG_ON
    vm_file* Logthread0;
    vm_file* Logthread1;
#endif
};

#endif
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
