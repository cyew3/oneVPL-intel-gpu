//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Cleanup.cpp
 *
 * Structures and routines for binary mask cleanup (as postprocessing)
 * 
 ********************************************************************************/
#include <stdio.h>
#include <limits.h>
#include "Cleanup.h"

//Pop element from stack
int pop(int *x, int *y, int pitch, int stack[], int& stackptr) 
{
    if(stackptr > 0) {
        int p = stack[stackptr];
        *x = p % pitch;
        *y = p / pitch;
        stackptr--;
        return 1;
    }
    else return 0;
}    

//Push element to stack
int push(int x, int y, int pitch, int stack[], int& stackptr, int MAX_BLK_RESOLUTION) 
{
    if(stackptr < MAX_BLK_RESOLUTION - 1) {
        stackptr++;
        stack[stackptr] = y*pitch + x;
        return 1;
    }
    else return 0;
}

//Empty stack
void empty_stack(int pitch, int stack[], int& stackptr) 
{
    int x, y; 
    while(pop(&x, &y, pitch, stack, stackptr));
}

//Non-recursively flood-fills a 4-connected component in a block
void flood_fill4(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int stack[], int& stackptr, int MAX_BLK_RESOLUTION) 
{
    if(nc == oc) return; //avoid infinite loop
    empty_stack(w, stack, stackptr);

    if(!push(x, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;

    while(pop(&x, &y, w, stack, stackptr)) {
        frm[y*w + x] = nc;
        //4-way filling
        if (x+1 <w) {
            if (frm[y*w + (x+1)]==oc && frm[y*w + (x+1)]!=nc) {
                if(!push(x+1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (x-1>=0) {
            if (frm[y*w + (x-1)]==oc && frm[y*w + (x-1)]!=nc) {
                if(!push(x-1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y+1 <h) { 
            if (frm[(y+1)*w + x]==oc && frm[(y+1)*w + x]!=nc) {
                if(!push(x, y+1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y-1>=0) {
            if (frm[(y-1)*w + x]==oc && frm[(y-1)*w + x]!=nc) {
                if(!push(x, y-1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
    }
}

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size
void flood_fill4_sz(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int stack[], int& stackptr, int MAX_BLK_RESOLUTION) 
{
    (*sz) = 0;
    if(nc == oc) return; //avoid infinite loop
    empty_stack(w, stack, stackptr);

    if(!push(x, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;

    while(pop(&x, &y, w, stack, stackptr)) {

        if (frm[y*w + x]!=nc) {
            frm[y*w + x] = nc;
            (*sz)++;
        }

        //4-way filling
        if (x+1 <w) {
            if (frm[y*w + (x+1)]==oc && frm[y*w + (x+1)]!=nc) {
                if(!push(x+1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (x-1>=0) {
            if (frm[y*w + (x-1)]==oc && frm[y*w + (x-1)]!=nc) {
                if(!push(x-1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y+1 <h) { 
            if (frm[(y+1)*w + x]==oc && frm[(y+1)*w + x]!=nc) {
                if(!push(x, y+1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y-1>=0) {
            if (frm[(y-1)*w + x]==oc && frm[(y-1)*w + x]!=nc) {
                if(!push(x, y-1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
    }
}

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size and bounding box
void flood_fill4_sz_bb(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int *min_x, int *max_x, int *min_y, int *max_y, int stack[], int& stackptr, int MAX_BLK_RESOLUTION) 
{
    (*sz) = 0;
    (*max_x) = (*max_y) = 0; 
    (*min_x) = (*min_y) = INT_MAX;
    if(nc == oc) return; //avoid infinite loop
    empty_stack(w, stack, stackptr);

    if(!push(x, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;

    while(pop(&x, &y, w, stack, stackptr)) {

        if (frm[y*w + x]!=nc) {
            frm[y*w + x] = nc;
            (*sz)++;
            if ((*min_x) > x) (*min_x) = x;
            if ((*max_x) < x) (*max_x) = x;
            if ((*min_y) > y) (*min_y) = y;
            if ((*max_y) < y) (*max_y) = y;
        }

        //4-way filling
        if (x+1 <w) {
            if (frm[y*w + (x+1)]==oc && frm[y*w + (x+1)]!=nc) {
                if(!push(x+1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (x-1>=0) {
            if (frm[y*w + (x-1)]==oc && frm[y*w + (x-1)]!=nc) {
                if(!push(x-1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y+1 <h) { 
            if (frm[(y+1)*w + x]==oc && frm[(y+1)*w + x]!=nc) {
                if(!push(x, y+1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y-1>=0) {
            if (frm[(y-1)*w + x]==oc && frm[(y-1)*w + x]!=nc) {
                if(!push(x, y-1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
    }
}

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size, bounding box and sums of elements from the masks
void flood_fill4_sz_bb_sum(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int *min_x, int *max_x, int *min_y, int *max_y, BYTE *mask, int *sum, BYTE *mask2, int *sum2, int stack[], int& stackptr, int MAX_BLK_RESOLUTION) 
{
    (*sz) = 0;
    (*sum) = 0;
    (*sum2) = 0;
    (*max_x) = (*max_y) = 0; 
    (*min_x) = (*min_y) = INT_MAX;
    if(nc == oc) return; //avoid infinite loop
    empty_stack(w, stack, stackptr);

    if(!push(x, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;

    while(pop(&x, &y, w, stack, stackptr)) {

        if (frm[y*w + x]!=nc) {
            frm[y*w + x] = nc;
            (*sz)++;
            (*sum)+= mask[y*w + x];
            (*sum2)+= mask2[y*w + x];
            if ((*min_x) > x) (*min_x) = x;
            if ((*max_x) < x) (*max_x) = x;
            if ((*min_y) > y) (*min_y) = y;
            if ((*max_y) < y) (*max_y) = y;
        }

        //4-way filling
        if (x+1 <w) {
            if (frm[y*w + (x+1)]==oc && frm[y*w + (x+1)]!=nc) {
                if(!push(x+1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (x-1>=0) {
            if (frm[y*w + (x-1)]==oc && frm[y*w + (x-1)]!=nc) {
                if(!push(x-1, y, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y+1 <h) { 
            if (frm[(y+1)*w + x]==oc && frm[(y+1)*w + x]!=nc) {
                if(!push(x, y+1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
        if (y-1>=0) {
            if (frm[(y-1)*w + x]==oc && frm[(y-1)*w + x]!=nc) {
                if(!push(x, y-1, w, stack, stackptr, MAX_BLK_RESOLUTION)) return;
            }
        }
    }
}

//Smooths the 0/255 binary mask with 1-pel smoothing size
void smooth_mask(BYTE *mask, int w, int h, int pitch) {
    int i, j;

    for (i=0; i<h; i++) {
        for (j=1; j<w-1; j++) {
            if (mask[i*pitch + j-1]==mask[i*pitch + j+1]) {
                mask[i*pitch + j] = mask[i*pitch + j+1];
            }
        }
    }
    for (i=1; i<h-1; i++) {
        for (j=0; j<w; j++) {
            if (mask[(i-1)*pitch + j]==mask[(i+1)*pitch + j]) {
                mask[i*pitch + j] = mask[(i+1)*pitch + j];
            }
        }
    }
}

