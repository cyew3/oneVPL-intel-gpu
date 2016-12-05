//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: FrameBuff.cpp
 *
 * Functions for manipulating frame buffer
 * 
 ********************************************************************************/
#include "Def.h"
#include "FrameBuff.h"

//initialization of frame buffer
int InitFrameBuffPtr(FrameBuffElementPtr **frameBuff, Dim *dim, int mode, int cc) {
    FS_UNREFERENCED_PARAMETER(mode);
    FrameBuffElementPtr *el;

    INIT_MEMORY_C(*frameBuff, 0, dim->numFr, FrameBuffElementPtr);
    if (*frameBuff == NULL) return 1;

    el = *frameBuff;

    for (int i = 0; i < (int) dim->numFr; i++) {
        el[i].frameY = NULL;
        el[i].frameU = NULL;
        el[i].frameV = NULL;
        el[i].w = 0;
        el[i].h = 0;
        el[i].p = 0;
        el[i].pc = 0;
        el[i].nv12 = cc;
        INIT_MEMORY_C(el[i].bSAD  , 0, dim->blTotal, BYTE);
        if (el[i].bSAD == NULL) return 1;
    }
    return 0;
}

//de-initialization of frame buffer
void DeinitFrameBuffPtr(FrameBuffElementPtr **frameBuff, Dim* dim) {
    FrameBuffElementPtr *el = *frameBuff;
    uint i;

    for (i = 0; i < dim->numFr; i++) {
        DEINIT_MEMORY_C(el[i].bSAD );
    }

    DEINIT_MEMORY_C(*frameBuff);
}

//cyclic swapping of pointers of cFramesStackElement type
void SwapFrameBuffPtrStackPointers(FrameBuffElementPtr **p, int num) {
    FrameBuffElementPtr *tmp; int i;
    if (num > 1) {
        tmp = p[num - 1];
        for (i = num - 1; i > 0; i--) {
            p[i] = p[i-1];
        }
        p[0] = tmp;
    }
}

