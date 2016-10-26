//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2014 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_THREAD_TASK_H)
#define __MFX_THREAD_TASK_H

#include <mfxvideo.h>
#include <mfxvideo++int.h>
#include "mfxenc.h"
#include "mfxpak.h"

// Declare task's parameters structure
typedef
struct MFX_THREAD_TASK_PARAMETERS
{
    union
    {
        // ENCODE parameters
        struct
        {
            mfxEncodeCtrl *ctrl;                                // [IN] pointer to encode control
            mfxFrameSurface1 *surface;                          // [IN] pointer to the source surface
            mfxBitstream *bs;                                   // [OUT] pointer to the destination bitstream
            mfxEncodeInternalParams internal_params;            // output of EncodeFrameCheck(), input of EncodeFrame()

        } encode;

        // DECODE parameters
        struct
        {
            mfxBitstream *bs;                                   // [IN] pointer to the source bitstream
            mfxFrameSurface1 *surface_work;                     // [IN] pointer to the surface for decoding
            mfxFrameSurface1 *surface_out;                      // [OUT] pointer to the current being decoded surface

        } decode;

        // VPP parameters
        struct
        {
            mfxFrameSurface1 *in;                               // [IN] pointer to the source surface
            mfxFrameSurface1 *out;                              // [OUT] pointer to the destination surface
            mfxExtVppAuxData *aux;                              // [IN] auxilary encoding data

        } vpp;

        // ENC, PAK parameters
        struct
        {
            mfxFrameCUC *cuc;                                   // [IN, OUT] pointer to encoding parameters
            void *pBRC;                                         // [IN] pointer to the BRC object
        } enc, pak;
        struct
        {
            mfxENCInput  *in; 
            mfxENCOutput *out;
        } enc_ext;
        struct
        {
            mfxPAKInput  *in;
            mfxPAKOutput *out;
        } pak_ext;
    };

} MFX_THREAD_TASK_PARAMETERS;

#endif // __MFX_THREAD_TASK_H
