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

