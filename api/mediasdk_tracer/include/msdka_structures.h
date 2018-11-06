/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: msdka_structures.h

\* ****************************************************************************** */

#ifndef __SDK_ANALYZER_H__
#define __SDK_ANALYZER_H__
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include "mfxvideo.h"
#include "mfxplugin.h"
#include "mfxenc.h"
#include "mfxla.h"
#include <vector>


enum   Component {
    DECODE  = 0x1,
    ENCODE  = 0x2,
    ENC     = 0x3,
    VPP     = 0x4,
    VPPEX   = 0x5,
};

enum   Direction{
    DIR_INPUT  = 0x100,
    DIR_OUTPUT = 0x200,
    DIR_HEADER = 0x400
};

//flag that not required syncronisation to record raw data
enum {
    FORCE_NO_SYNC  = 0x4000
} ;

//entry points where framesurface is used
enum   ComponentEntryPoint {
    RunAsync_Input,
    RunAsync_Output,
    SyncOperation_Input,
    SyncOperation_Output,
};

struct AnalyzerSession {
    mfxSession  session;
    mfxI64      init_tick;
    mfxFrameAllocator *pAlloc;

    struct Statistics {
        mfxI64 async_exec_rate;
        mfxI64 async_exec_time;
        mfxI64 exec_time;
        mfxU64 frames;
        mfxU64 frames_sync;
        mfxI64 last_async_tick;
        mfxBitstream first_frame;
        std::vector<mfxU8> bsData;
        Statistics()
            : first_frame()
            , async_exec_rate()
            , async_exec_time()
            , exec_time()
            , frames()
            , frames_sync()
            , last_async_tick()
        {
        }
    } encode, enc, decode, vpp;

    AnalyzerSession()
        : session()
        , init_tick()
        , pAlloc()
    {
    }
};

struct AnalyzerBufferAllocator:mfxBufferAllocator {
    mfxBufferAllocator *allocator;

    static mfxStatus AllocFunc(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    static mfxStatus LockFunc(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    static mfxStatus UnlockFunc(mfxHDL pthis, mfxMemId mid);
    static mfxStatus FreeFunc(mfxHDL pthis, mfxMemId mid);
  
    AnalyzerBufferAllocator(mfxBufferAllocator *ba);
};


struct AnalyzerFrameAllocator : mfxFrameAllocator {
    mfxFrameAllocator *allocator;

    struct SurfacesUsageStat{
        mfxU16 nSurfacesAllocated;
        mfxU16 nSurfacesLocked;
        std::vector<mfxFrameSurface1 *> surfaces;
        
        SurfacesUsageStat()
            : nSurfacesAllocated()
            , nSurfacesLocked() {}
        mfxU16 GetNumLockedSurfaces();
        mfxU16 GetCachedNumLockedSurfaces();
        void RegisterSurface(mfxFrameSurface1*);
    }decode, encode, enc;

    static mfxStatus AllocFunc(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response); 
    static mfxStatus LockFunc(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus UnlockFunc(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    static mfxStatus GetHDLFunc(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    static mfxStatus FreeFunc(mfxHDL pthis, mfxFrameAllocResponse *response);
    AnalyzerFrameAllocator(mfxFrameAllocator *fa);
};


struct AnalyzerSyncPoint {
    mfxSyncPoint sync_point;
    bool copy_data_from_stub;
    Component c;
    mfxI64  start_tick;
    mfxStatus sts;

    union {
        mfxBitstream *bitstream;
        mfxFrameSurface1 *psurface_out;
        struct {
            mfxFrameSurface1 *out;
            mfxExtVppAuxData *aux;
        };
        struct {
            mfxFrameSurface1 *work;
            //MFXVideoVPP_RunFrameVPPAsyncEx output
            mfxFrameSurface1 **ppOut;
        };
    } output;

    union {
        mfxFrameSurface1 *frame;
    } input;

    AnalyzerSyncPoint(Component c) {
        sync_point=0;
        start_tick=0;
        sts=MFX_ERR_NONE;
        copy_data_from_stub = false;
        this->c=c;
        memset(&output,0,sizeof(output));
        memset(&input,0,sizeof(input));
    }
};

struct ExtendedBufferOverride {
    mfxExtBuffer **ExtParam_old;
    mfxU16 NumExtParam_old;
    mfxU32 *AlgList_old;
    mfxU32 NumAlg_old;
    mfxU32 **pAlgList;
    mfxU32 *pNumAlg;
    mfxVideoParam *par;

    ExtendedBufferOverride(mfxVideoParam *par) {
        this->par=par;
        ExtParam_old = NULL;
        if (par) ExtParam_old=par->ExtParam;
        pAlgList=0;
    }
    ~ExtendedBufferOverride(void) {
        if (pAlgList) {
            delete *pAlgList;
            *pAlgList=AlgList_old;
            *pNumAlg=NumAlg_old;
        }
        if (par) if (par->ExtParam != ExtParam_old && NULL != ExtParam_old ) {
            for (int i=0;i<(int)par->NumExtParam;i++) {
                bool found=false;
                for (int j=0;j<(int)NumExtParam_old;j++)
                    if (par->ExtParam && par->ExtParam[i]==ExtParam_old[j]) found=true;
                if (par->ExtParam && !found) delete par->ExtParam[i];
            }
            delete par->ExtParam;
            par->ExtParam=ExtParam_old;
            par->NumExtParam=NumExtParam_old;
        }
    }
};

struct StatusOverride {
    mfxStatus if_status;
    mfxStatus then_status;
    bool then_forced;
    bool if_forced;

    StatusOverride(void) {
        if_forced=then_forced=false;
    }

    mfxStatus Override(mfxStatus sts) {
        if (!then_forced) return sts;
        if (!if_forced) return then_status;
        return (sts==if_status)?then_status:sts;
    }
};


/* declare internal parameters */
class  StatusMap {
public:
    void Count(mfxStatus sts) {
        if (sts>= -128 && sts<128) counter[sts+128]++;
    }
    void CountInternal(mfxStatus sts) {
        if (sts>= -128 && sts<128) counterInternal[sts+128]++;
    }

    bool Traverse(int i, mfxStatus *sts, int *ct) {
        if (i<0 || i>255) return false;
        *ct=counter[i]; *sts=(mfxStatus)(i-128);
        return true;            
    }
    bool TraverseInternal(int i, mfxStatus *sts, int *ct) {
        if (i<0 || i>255) return false;
        *ct=counterInternal[i]; *sts=(mfxStatus)(i-128);
        return true;            
    }
    StatusMap(void) {
        memset(&counter,0,sizeof(counter));
        memset(&counterInternal,0,sizeof(counterInternal));
    }
protected:
    int counter[256];
    //internal statuses are not passed to application
    int counterInternal[256];
};

struct LinkedString {
    LinkedString *next;
    TCHAR *string;
};

struct DebugBuffer {
    int dwLevel;
    CHAR szString[4*1024 - sizeof(DWORD)];
};

#endif
