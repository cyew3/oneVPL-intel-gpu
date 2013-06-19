/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: mfxaudio++int.h

\* ****************************************************************************** */
#ifndef __MFXAUDIOPLUSPLUS_INTERNAL_H
#define __MFXAUDIOPLUSPLUS_INTERNAL_H

#include "mfxaudio.h"
#include <mfx_task_threading_policy.h>
#include <mfx_interface.h>



#if defined(_WIN32) || defined(_WIN64)
#include <guiddef.h>
#else
#ifndef GUID_TYPE_DEFINED

#include <string.h>
// TODO: temporary workaround for linux (aya: see below)
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;

static int operator==(const GUID & guidOne, const GUID & guidOther)
{
    return !memcmp(&guidOne, &guidOne, sizeof(GUID));
}
#define GUID_TYPE_DEFINED
#endif


// GUIDs from DDI spec 0.73
static const GUID DXVA2_Intel_Encode_AVC = 
{ 0x97688186, 0x56a8, 0x4094, { 0xb5, 0x43, 0xfc, 0x9d, 0xaa, 0xa4, 0x9f, 0x4b } };
static const GUID DXVA2_Intel_Encode_MPEG2 = 
{ 0xc346e8a3, 0xcbed, 0x4d27, { 0x87, 0xcc, 0xa7, 0xe, 0xb4, 0xdc, 0x8c, 0x27 } };
static const GUID DXVA2_Intel_Encode_SVC = 
{ 0xd41289c2, 0xecf3, 0x4ede, { 0x9a, 0x04, 0x3b, 0xbf, 0x90, 0x68, 0xa6, 0x29 } };

static const GUID sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT =
{ 0x91cd2d6e, 0x897b, 0x4fa1, { 0xb0, 0xd7, 0x51, 0xdc, 0x88, 0x01, 0x0e, 0x0a } };

static const GUID sDXVA2_ModeMPEG2_VLD =
{ 0xee27417f, 0x5e28, 0x4e65, { 0xbe, 0xea, 0x1d, 0x26, 0xb5, 0x08, 0xad, 0xc9 } };

static const GUID sDXVA2_Intel_ModeVC1_D_Super =
{ 0xE07EC519, 0xE651, 0x4cd6, { 0xAC, 0x84, 0x13, 0x70, 0xCC, 0xEE, 0xC8, 0x51 } };

static const GUID sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT =
{ 0x604f8e68, 0x4951, 0x4c54, { 0x88, 0xfe, 0xab, 0xd2, 0x5c, 0x15, 0xb3, 0xd6 } };

static const GUID sDXVA_ModeH264_VLD_Multiview_NoFGT =
{ 0x705b9d82, 0x76cf, 0x49d6, { 0xb7, 0xe6, 0xac, 0x88, 0x72, 0xdb, 0x01, 0x3c } };

static const GUID sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT =
{ 0xd79be8da, 0x0cf1, 0x4c81, { 0xb8, 0x2a, 0x69, 0xa4, 0xe2, 0x36, 0xf4, 0x3d } };

static const GUID sDXVA_ModeH264_VLD_Stereo_NoFGT =
{ 0xf9aaccbb, 0xc2b6, 0x4cfc, { 0x87, 0x79, 0x57, 0x07, 0xb1, 0x76, 0x05, 0x52 } };

static const GUID sDXVA2_ModeH264_VLD_NoFGT =
{ 0x1b81be68, 0xa0c7, 0x11d3, { 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 } };

static const GUID sDXVA_Intel_ModeH264_VLD_MVC =
{ 0xe296bf50, 0x8808, 0x4ff8, { 0x92, 0xd4, 0xf1, 0xee, 0x79, 0x9f, 0xc3, 0x3c } };

#endif

#if defined(_WIN32) || defined(_WIN64)
#undef MFX_DEBUG_TOOLS
#define MFX_DEBUG_TOOLS

#if defined(DEBUG) || defined(_DEBUG)
#undef  MFX_DEBUG_TOOLS // to avoid redefinition
#define MFX_DEBUG_TOOLS
#endif
#endif // #if defined(_WIN32) || defined(_WIN64)




// This is the include file for Media SDK component development.
enum eMFXAudioPlatform
{
    MFX_AUDIO_PLATFORM_SOFTWARE      = 0,
    MFX_AUDIO_PLATFORM_HARDWARE      = 1,
};





#ifdef MFX_DEBUG_TOOLS
// commented since running behavior tests w/o catch is very annoying
// #define MFX_CORE_CATCH_TYPE     int**** // to disable catch
#define MFX_CORE_CATCH_TYPE     ...
#else
#define MFX_CORE_CATCH_TYPE     ...
#endif

// Forward declaration of used classes
struct MFX_ENTRY_POINT;
 
// AudioCORE
class AudioCORE {
public:

    virtual ~AudioCORE(void) {};

    // imported to external API
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *handle) = 0;
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL handle) = 0;
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *allocator) = 0;
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator) = 0;

    // Internal interface only
    // Utility functions for memory access
    virtual mfxStatus  AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid) = 0;
    virtual mfxStatus  LockBuffer(mfxMemId mid, mfxU8 **ptr) = 0;
    virtual mfxStatus  UnlockBuffer(mfxMemId mid) = 0;
    virtual mfxStatus  FreeBuffer(mfxMemId mid) = 0;

    
        
    virtual mfxStatus  AllocFrames(mfxFrameAllocRequest *request, 
                                   mfxFrameAllocResponse *response) = 0;

    virtual mfxStatus  AllocFrames(mfxFrameAllocRequest *request, 
                                   mfxFrameAllocResponse *response, 
                                   mfxFrameSurface1 **pOpaqueSurface, 
                                   mfxU32 NumOpaqueSurface) = 0;

    virtual mfxStatus  LockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus  UnlockFrame(mfxMemId mid, mfxFrameData *ptr=0) = 0;
    virtual mfxStatus  FreeFrames(mfxFrameAllocResponse *response, bool ExtendedSearch = true) = 0;

    virtual mfxStatus  LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch = true) = 0;
    virtual mfxStatus  UnlockExternalFrame(mfxMemId mid, mfxFrameData *ptr=0, bool ExtendedSearch = true) = 0;

    virtual mfxMemId MapIdx(mfxMemId mid) = 0;


    // Increment Surface lock
    virtual mfxStatus  IncreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true) = 0;
    // Decrement Surface lock
    virtual mfxStatus  DecreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true) = 0;

    // no care about surface, opaq and all round. Just increasing reference
    virtual mfxStatus IncreasePureReference(mfxFrameData *ptr) = 0;
    // no care about surface, opaq and all round. Just decreasing reference
    virtual mfxStatus DecreasePureReference(mfxFrameData *ptr) = 0;
    virtual void* QueryCoreInterface(const MFX_GUID &guid) = 0;
    virtual mfxSession GetSession() = 0;
};     


class AudioENCODE
{
public:
    // Destructor
    virtual
    ~AudioENCODE(void) {}
   
    virtual
    mfxStatus Init(mfxAudioParam *par) = 0;
    virtual
    mfxStatus Reset(mfxAudioParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetAudioParam(mfxAudioParam *par) = 0;
    virtual
    mfxStatus EncodeFrameCheck(mfxBitstream *bs, mfxBitstream *buffer_out) = 0;
    virtual
    mfxStatus EncodeFrame(mfxBitstream *bs, mfxBitstream *buffer_out) = 0;

    virtual mfxStatus EncodeFrameCheck(mfxBitstream *bs, mfxBitstream *buffer_out,
        MFX_ENTRY_POINT *pEntryPoint)  = 0;
  
};

class AudioDECODE
{
public:
    // Destructor
    virtual
    ~AudioDECODE(void) {}

    virtual
    mfxStatus Init(mfxAudioParam *par) = 0;
    virtual
    mfxStatus Reset(mfxAudioParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetAudioParam(mfxAudioParam *par) = 0;
    virtual
    mfxStatus DecodeFrameCheck(mfxBitstream *bs,  mfxBitstream *buffer_out,
        MFX_ENTRY_POINT *pEntryPoint)
    {
        pEntryPoint = pEntryPoint;
        return DecodeFrameCheck(bs, buffer_out);
    }
    virtual
    mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxBitstream *buffer_out) = 0;
    virtual
    mfxStatus DecodeFrame(mfxBitstream *bs, mfxBitstream *buffer_out) = 0;
 protected:



};
   /*
class AudioPP
{
public:
    // Destructor
    virtual
    ~AudioPP(void) {}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetVPPStat(mfxVPPStat *stat) = 0;
    virtual
    mfxStatus AudioppFrameCheck(mfxFrameSurface1 *in,
                            mfxFrameSurface1 *out,
                            mfxExtVppAuxData *aux,
                            MFX_ENTRY_POINT *pEntryPoint)
    {
        pEntryPoint = pEntryPoint;
        aux = aux;
        return AudioppFrameCheck(in, out);
    }

    virtual
    mfxStatus AudioppFrameCheck(mfxFrameSurface1 *in,
                            mfxFrameSurface1 *out,
                            mfxExtVppAuxData *aux,
                            MFX_ENTRY_POINT pEntryPoints[],
                            mfxU32 &numEntryPoints)
    {
        mfxStatus mfxRes;

        // call the overweighted version
        mfxRes = VppFrameCheck(in, out, aux, pEntryPoints);
        numEntryPoints = 1;

        return mfxRes;
    }

    virtual
    mfxStatus AudioppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out) = 0;
    virtual
    mfxStatus RunFrameAudioPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux) = 0;

};

   */

#endif // __MFXAUDIOPLUSPLUS_INTERNAL_H
