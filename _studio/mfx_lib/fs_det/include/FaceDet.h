//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
* 
* File: FaceDet.h
*
* Routines for Viola-Jones face detection
* 
********************************************************************************/
#ifndef _FACE_DET_H_
#define _FACE_DET_H_

#include "Def.h"
#include <vector>

//define pruning threshold
#define T_PR 0.2

//define min and max without jumps
#define  FMIN(a, b)  ((a) ^ (((a)^(b)) & (((a) < (b)) - 1)))
#define  FMAX(a, b)  ((a) ^ (((a)^(b)) & (((a) > (b)) - 1)))

//define point structure
typedef struct {
    int x;
    int y;
} Pt;

//define rectangle structure
typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rct;

//haar rectangle (cascade->stage->classifier->feature->rect)
typedef struct {
    int x;
    int y;
    int width;
    int height;
    double weight;
} haar_rect;

//haar freature (cascade->stage->classifier->feature)
typedef struct {
    int num_rects;
    haar_rect rect[3];
    int tilted;
} haar_feature;

//haar classifier (cascade->stage->classifier)
typedef struct {
    haar_feature feature;
    double threshold;
    double alpha[2];
} haar_classifier;

//haar stage (cascade->stage)
typedef struct {
    int num_classifiers;
    const haar_classifier *classifier;
    double stage_threshold;
} haar_stage;

//haar cascade
typedef struct {
    int num_stages;
    int win_width;
    int win_height;
    int scaled_win_width;
    int scaled_win_height;
    double scl;
    const haar_stage *stage;
} haar_cascade;

//haarclass rectangle
typedef struct {
    int x;
    int y;
    int width;
    int height;
    double weight;
    int *a, *b, *c, *d;
} haarclass_rect;

//haarclass feature
typedef struct {
    int num_rects;
    haarclass_rect rect[3];
} haarclass_feature;

//haarclass classifier
typedef struct {
    haarclass_feature feature;
    double threshold;
    double alpha[2];
} haarclass_classifier;

//haarclass stage
typedef struct haarclass_stage {
    int num_classifiers;
    double stage_threshold;
    haarclass_classifier* classifier;
    int simple;
} haarclass_stage;

//haarclass cascade
typedef struct {
    int num_stages;
    double inv_window_area;
    int *sum;
    double *sqsum;
    haarclass_stage *stage;
    double *A, *B, *C, *D;
    int    *a, *b, *c, *d;
} haarclass_cascade;

//normalize contrast
void normalize_contrast(BYTE *src, BYTE *dst, int w, int h);


//create haar classifier
int init_haar_classifier(haarclass_cascade **res, const haar_cascade *cascade);

//round double to int
static const double magic_double = 6755399441055744.0;  // 2^52 + 2^51
inline int round_double(double x) {
    x += magic_double;
    return reinterpret_cast<int&>(x);
}

//face detection
void do_face_detection(std::vector<Rct> &faces, BYTE *frm, int w, int h, haar_cascade *cascade, haarclass_cascade *class_cascade, int *sum, double *sqsum, int mwin_w, int mwin_h, int min_nboors);

#endif