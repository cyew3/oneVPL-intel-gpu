#pragma once

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_encode_api.h"
#include "mfxla.h"

namespace ApiTestCommon {
    template<class T> struct ExtBufInfo;
    template<> struct ExtBufInfo<mfxExtOpaqueSurfaceAlloc> { enum { id = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sz =  sizeof(mfxExtOpaqueSurfaceAlloc) }; };
    template<> struct ExtBufInfo<mfxExtCodingOptionHEVC>   { enum { id = MFX_EXTBUFF_HEVCENC,                   sz =  sizeof(mfxExtCodingOptionHEVC) }; };
    template<> struct ExtBufInfo<mfxExtDumpFiles>          { enum { id = MFX_EXTBUFF_DUMP,                      sz =  sizeof(mfxExtDumpFiles) }; };
    template<> struct ExtBufInfo<mfxExtHEVCTiles>          { enum { id = MFX_EXTBUFF_HEVC_TILES,                sz =  sizeof(mfxExtHEVCTiles) }; };
    template<> struct ExtBufInfo<mfxExtHEVCParam>          { enum { id = MFX_EXTBUFF_HEVC_PARAM,                sz =  sizeof(mfxExtHEVCParam) }; };
    template<> struct ExtBufInfo<mfxExtHEVCRegion>         { enum { id = MFX_EXTBUFF_HEVC_REGION,               sz =  sizeof(mfxExtHEVCRegion) }; };
    template<> struct ExtBufInfo<mfxExtCodingOption>       { enum { id = MFX_EXTBUFF_CODING_OPTION,             sz =  sizeof(mfxExtCodingOption) }; };
    template<> struct ExtBufInfo<mfxExtCodingOption2>      { enum { id = MFX_EXTBUFF_CODING_OPTION2,            sz =  sizeof(mfxExtCodingOption2) }; };
    template<> struct ExtBufInfo<mfxExtCodingOption3>      { enum { id = MFX_EXTBUFF_CODING_OPTION3,            sz =  sizeof(mfxExtCodingOption3) }; };
    template<> struct ExtBufInfo<mfxExtCodingOptionSPSPPS> { enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS,      sz =  sizeof(mfxExtCodingOptionSPSPPS) }; };
    template<> struct ExtBufInfo<mfxExtCodingOptionVPS>    { enum { id = MFX_EXTBUFF_CODING_OPTION_VPS,         sz =  sizeof(mfxExtCodingOptionVPS) }; };
    template<> struct ExtBufInfo<mfxExtLAFrameStatistics>  { enum { id = MFX_EXTBUFF_LOOKAHEAD_STAT,            sz =  sizeof(mfxExtLAFrameStatistics) }; };
    template<> struct ExtBufInfo<mfxExtEncoderROI>         { enum { id = MFX_EXTBUFF_ENCODER_ROI,               sz =  sizeof(mfxExtEncoderROI) }; };

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

    struct ParamSet {
        ParamSet();
        mfxExtOpaqueSurfaceAlloc extOpaqueSurfaceAlloc;
        mfxExtCodingOptionHEVC   extCodingOptionHevc;
        mfxExtDumpFiles          extDumpFiles;
        mfxExtHEVCTiles          extHevcTiles;
        mfxExtHEVCParam          extHevcParam;
        mfxExtHEVCRegion         extHevcRegion;
        mfxExtCodingOption       extCodingOption;
        mfxExtCodingOption2      extCodingOption2;
        mfxExtCodingOption3      extCodingOption3;
        mfxExtEncoderROI         extEncoderROI; 
        mfxExtBuffer            *extBuffers[20];
        mfxVideoParam            videoParam;
    };

    template <class T> void ExpectEqual(const T &expected, const T &actual);
    template <> void ExpectEqual<mfxInfoMFX>(const mfxInfoMFX &expected, const mfxInfoMFX &actual);
    template <> void ExpectEqual<mfxExtOpaqueSurfaceAlloc>(const mfxExtOpaqueSurfaceAlloc &expected, const mfxExtOpaqueSurfaceAlloc &actual);
    template <> void ExpectEqual<mfxExtCodingOptionHEVC>(const mfxExtCodingOptionHEVC &expected, const mfxExtCodingOptionHEVC &actual);
    template <> void ExpectEqual<mfxExtDumpFiles>(const mfxExtDumpFiles &expected, const mfxExtDumpFiles &actual);
    template <> void ExpectEqual<mfxExtHEVCTiles>(const mfxExtHEVCTiles &expected, const mfxExtHEVCTiles &actual);
    template <> void ExpectEqual<mfxExtHEVCParam>(const mfxExtHEVCParam &expected, const mfxExtHEVCParam &actual);
    template <> void ExpectEqual<mfxExtHEVCRegion>(const mfxExtHEVCRegion &expected, const mfxExtHEVCRegion &actual);
    template <> void ExpectEqual<mfxExtCodingOption>(const mfxExtCodingOption &expected, const mfxExtCodingOption &actual);
    template <> void ExpectEqual<mfxExtCodingOption2>(const mfxExtCodingOption2 &expected, const mfxExtCodingOption2 &actual);
    template <> void ExpectEqual<mfxExtCodingOption3>(const mfxExtCodingOption3 &expected, const mfxExtCodingOption3 &actual);
    template <> void ExpectEqual<mfxExtEncoderROI>(const mfxExtEncoderROI &expected, const mfxExtEncoderROI &actual);
    template <> void ExpectEqual<ParamSet>(const ParamSet &expected, const ParamSet &actual);

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
