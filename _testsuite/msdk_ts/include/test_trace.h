#pragma once
#include <iostream>
#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxsvc.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxpcp.h"
//#include "mfxaudio.h"

enum print_flags{
    PRINT_OPT_ENC   = 0x00000001,
    PRINT_OPT_DEC   = 0x00000002,
    PRINT_OPT_VPP   = 0x00000004,
    PRINT_OPT_AVC   = 0x00000010,
    PRINT_OPT_MPEG2 = 0x00000020,
    PRINT_OPT_VC1   = 0x00000040,
    PRINT_OPT_VP8   = 0x00000080,
    PRINT_OPT_JPEG  = 0x00000100,
};

struct _print_param{
    mfxU32 flags;
    std::string padding;
};
extern _print_param print_param;
#define INC_PADDING() print_param.padding += "  ";
#define DEC_PADDING() print_param.padding.erase(print_param.padding.size() - 2, 2);

typedef struct{
    unsigned char* data;
    unsigned int size;
} rawdata;

rawdata hexstr(void* d, unsigned int s);

std::ostream &operator << (std::ostream &os, rawdata p);
std::ostream &operator << (std::ostream &os, std::string &p);
std::ostream &operator << (std::ostream &os, mfxU8* &p);
std::ostream &operator << (std::ostream &os, mfxStatus &st);
std::ostream &operator << (std::ostream &os, mfxVideoParam* &p);
std::ostream &operator << (std::ostream &os, mfxVideoParam &p);
std::ostream &operator << (std::ostream &os, mfxInfoMFX &p);
std::ostream &operator << (std::ostream &os, mfxInfoVPP &p);
std::ostream &operator << (std::ostream &os, mfxFrameInfo &p);
std::ostream &operator << (std::ostream &os, mfxFrameId &p);
std::ostream &operator << (std::ostream &os, mfxVersion* &p);
std::ostream &operator << (std::ostream &os, mfxVersion &p);
std::ostream &operator << (std::ostream &os, mfxSession* &p);
std::ostream &operator << (std::ostream &os, mfxFrameAllocRequest &p);
std::ostream &operator << (std::ostream &os, mfxFrameAllocRequest* &p);
std::ostream &operator << (std::ostream &os, mfxSyncPoint* &p);
std::ostream &operator << (std::ostream &os, mfxBitstream &p);
std::ostream &operator << (std::ostream &os, mfxBitstream* &p);
std::ostream &operator << (std::ostream &os, mfxFrameSurface1 &p);
std::ostream &operator << (std::ostream &os, mfxFrameSurface1* &p);
std::ostream &operator << (std::ostream &os, mfxFrameSurface1** &p);
std::ostream &operator << (std::ostream &os, mfxFrameData &p);
std::ostream &operator << (std::ostream &os, mfxExtBuffer &p);
std::ostream &operator << (std::ostream &os, mfxExtBuffer* &p);
std::ostream &operator << (std::ostream &os, mfxExtSVCSeqDesc &p);
std::ostream &operator << (std::ostream &os, mfxExtSVCSeqDesc* &p);
std::ostream &operator << (std::ostream &os, mfxExtSVCRateControl &p);
std::ostream &operator << (std::ostream &os, mfxExtSVCRateControl* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDoNotUse &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDoNotUse* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDenoise &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDenoise* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDetail &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDetail* &p);
std::ostream &operator << (std::ostream &os, mfxExtCodingOption2 &p);
std::ostream &operator << (std::ostream &os, mfxExtCodingOption2* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPProcAmp &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPProcAmp* &p);
std::ostream &operator << (std::ostream &os, mfxExtVppAuxData &p);
std::ostream &operator << (std::ostream &os, mfxExtVppAuxData* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDoUse &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPDoUse* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPFrameRateConversion &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPFrameRateConversion* &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPImageStab &p);
std::ostream &operator << (std::ostream &os, mfxExtVPPImageStab* &p);
std::ostream &operator << (std::ostream &os, mfxExtMVCSeqDesc &p);
std::ostream &operator << (std::ostream &os, mfxExtMVCSeqDesc* &p);
std::ostream &operator << (std::ostream &os, mfxExtCodingOption &p);
std::ostream &operator << (std::ostream &os, mfxExtCodingOption* &p);
std::ostream &operator << (std::ostream &os, mfxExtOpaqueSurfaceAlloc &p);
std::ostream &operator << (std::ostream &os, mfxExtOpaqueSurfaceAlloc* &p);
std::ostream &operator << (std::ostream &os, mfxPayload &p);
std::ostream &operator << (std::ostream &os, mfxPayload* &p);
std::ostream &operator << (std::ostream &os, mfxEncodeCtrl &p);
std::ostream &operator << (std::ostream &os, mfxEncodeCtrl* &p);
std::ostream &operator << (std::ostream &os, mfxAES128CipherCounter &p);
std::ostream &operator << (std::ostream &os, mfxAES128CipherCounter* &p);
std::ostream &operator << (std::ostream &os, mfxExtPAVPOption &p);
std::ostream &operator << (std::ostream &os, mfxExtPAVPOption* &p);
std::ostream &operator << (std::ostream &os, mfxEncryptedData &p);
std::ostream &operator << (std::ostream &os, mfxEncryptedData* &p);

#ifdef __MFXAUDIO_H__
std::ostream &operator << (std::ostream &os, mfxAudioStreamInfo &p);
std::ostream &operator << (std::ostream &os, mfxAudioStreamInfo* &p);
std::ostream &operator << (std::ostream &os, mfxInfoAudioMFX &p);
std::ostream &operator << (std::ostream &os, mfxInfoAudioMFX* &p);
std::ostream &operator << (std::ostream &os, mfxInfoAudioPP &p);
std::ostream &operator << (std::ostream &os, mfxInfoAudioPP* &p);
std::ostream &operator << (std::ostream &os, mfxAudioParam &p);
std::ostream &operator << (std::ostream &os, mfxAudioParam* &p);
std::ostream &operator << (std::ostream &os, mfxAudioAllocRequest &p);
std::ostream &operator << (std::ostream &os, mfxAudioAllocRequest* &p);
#endif //#ifdef __MFXAUDIO_H__

void allow_debug_output();
