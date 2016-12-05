//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Dim.c
 *
 * Structures and routines for video dimensions
 * 
 ********************************************************************************/
#include "Dim.h"

//Performs initialization of Dim structure (variables that depends on dimensions)
void cDimInit(Dim *dim, int w, int h, int blSz, int numFr) {
	dim->w		    = w;
	dim->h		    = h;

	dim->wb		    = w/blSz;
	dim->hb		    = h/blSz;
	dim->ubOff      = dim->hb * dim->wb;
	dim->vbOff      = dim->hb * dim->wb * 2;
	dim->frbSize    = dim->hb * dim->wb * 3;

	dim->blSz		= blSz;
	dim->blSzPx		= blSz * blSz;
	dim->blHNum		= w / blSz;
	dim->blVNum		= h / blSz;
	dim->blTotal	= dim->blHNum * dim->blVNum;

	dim->numFr      = numFr;
}
