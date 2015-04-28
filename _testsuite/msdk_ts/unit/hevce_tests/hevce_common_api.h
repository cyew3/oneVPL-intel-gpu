#pragma once

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_encode_api.h"

namespace ApiTestCommon {
    template<class T> struct ExtBufInfo;
    template<> struct ExtBufInfo<mfxExtOpaqueSurfaceAlloc> { enum { id = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sz =   80 }; };
    template<> struct ExtBufInfo<mfxExtCodingOptionHEVC>   { enum { id = MFX_EXTBUFF_HEVCENC,                   sz =  260 }; };
    template<> struct ExtBufInfo<mfxExtDumpFiles>          { enum { id = MFX_EXTBUFF_DUMP,                      sz = 1572 }; };
    template<> struct ExtBufInfo<mfxExtHEVCTiles>          { enum { id = MFX_EXTBUFF_HEVC_TILES,                sz =  160 }; };
    template<> struct ExtBufInfo<mfxExtHEVCParam>          { enum { id = MFX_EXTBUFF_HEVC_PARAM,                sz =  256 }; };
    template<> struct ExtBufInfo<mfxExtHEVCRegion>         { enum { id = MFX_EXTBUFF_HEVC_REGION,               sz =   64 }; };
    template<> struct ExtBufInfo<mfxExtCodingOption2>      { enum { id = MFX_EXTBUFF_CODING_OPTION2,            sz =   68 }; };
    template<> struct ExtBufInfo<mfxExtCodingOptionSPSPPS> { enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS,      sz =   0  }; };

    template <class T> T MakeExtBuffer() {
        T extBuffer = {};
        extBuffer.Header.BufferId = ExtBufInfo<T>::id;
        extBuffer.Header.BufferSz = ExtBufInfo<T>::sz;
        return extBuffer;
    }

    template <class T> void Zero(T& obj) {
        memset(&obj, 0, sizeof(T));
    }

    template <class T> int MemCompare(const T& buffer1, const T& buffer2) {
        return memcmp(&buffer1, &buffer2, sizeof(T));
    }

    template <class T> void ExpectEqual(const T &expected, const T &actual);

    struct ParamSet {
        ParamSet();
        mfxExtOpaqueSurfaceAlloc extOpaqueSurfaceAlloc;
        mfxExtCodingOptionHEVC   extCodingOptionHevc;
        mfxExtDumpFiles          extDumpFiles;
        mfxExtHEVCTiles          extHevcTiles;
        mfxExtHEVCParam          extHevcParam;
        mfxExtHEVCRegion         extHevcRegion;
        mfxExtCodingOption2      extCodingOption2;
        mfxExtBuffer            *extBuffers[10];
        mfxVideoParam            videoParam;
    };

    void SetupParamSet(ParamSet &paramset);
    void CopyParamSet(ParamSet &dst, const ParamSet &src);
    void InitParamSetMandated(ParamSet &paramset);
    void InitParamSetValid(ParamSet &paramset);
    void InitParamSetCorrectable(ParamSet &input, ParamSet &expectedOutput);
    void InitParamSetUnsupported(ParamSet &input, ParamSet &expectedOutput);


    struct Nal { const Ipp8u *start, *end; Ipp32u type; };
    std::vector<Nal> SplitNals(const Ipp8u *start, const Ipp8u *end);
    std::vector<Ipp8u> ExtractRbsp(const Ipp8u *inStart, const Ipp8u *inEnd);
};
