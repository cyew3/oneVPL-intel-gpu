/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_common.h"
#include "encoding_ddi.h"
#include "mfx_vp9_encode_hw_utils.h"

#if defined (AS_VP9E_PLUGIN)

namespace MfxHwVP9Encode
{

static const GUID DXVA2_Intel_Encode_VP9_Profile0 =
{ 0x464bdb3c, 0x91c4, 0x4e9b, { 0x89, 0x6f, 0x22, 0x54, 0x96, 0xac, 0x4e, 0xd6 } };
static const GUID DXVA2_Intel_LowpowerEncode_VP9_Profile0 =
{ 0x9b31316b, 0xf204, 0x455d, { 0x8a, 0x8c, 0x93, 0x45, 0xdc, 0xa7, 0x7c, 0x1 } };
static const GUID DXVA2_Intel_Encode_VP9_Profile1 =
{ 0xafe4285c, 0xab63, 0x4b2d, { 0x82, 0x78, 0xe6, 0xba, 0xac, 0xea, 0x2c, 0xe9 } };
static const GUID DXVA2_Intel_Encode_VP9_10bit_Profile2 =
{ 0x4c5ba10, 0x4e9a, 0x4b8e, { 0x8d, 0xbf, 0x4f, 0x4b, 0x48, 0xaf, 0xa2, 0x7c } };
static const GUID DXVA2_Intel_Encode_VP9_10bit_Profile3 =
{ 0x24d19fca, 0xc5a2, 0x4b8e, { 0x9f, 0x93, 0xf8, 0xf6, 0xef, 0x15, 0xc8, 0x90 } };

//#define DDI_VER_0960

#ifdef DDI_VER_0960
// DDI v0.960
typedef struct tagENCODE_CAPS_VP9
{
    union
    {
        struct
        {
            UINT    CodingLimitSet          : 1;
            UINT    Color420Only            : 1;
            UINT    SegmentationSupport     : 1;
            UINT    FrameLevelRateCtrl      : 1;
            UINT    BRCReset                : 1;
            UINT    MBBRCSupport            : 1;
            UINT    TemporalLayerRateCtrl   : 3;
            UINT    DynamicScaling          : 1;
            UINT    TileSupport             : 1;
            UINT    MaxLog2TileCols         : 3;
            UINT    YUV422ReconSupport      : 1;
            UINT    YUV444ReconSupport      : 1;
            UINT    MaxEncodedBitDepth      : 2;
            UINT    UserMaxFrameSizeSupport : 1;
            UINT                            : 13;
        };
        UINT CodingLimits;
    };

    union
    {
        struct
        {
            BYTE    EncodeFunc              : 1;  // Enc+Pak
            BYTE    HybridPakFunc           : 1;  // Hybrid Pak function
            BYTE    EncFunc                 : 1;  // Enc only function
            BYTE                            : 6;
        };
        BYTE CodingFunction;
    };
    UINT    MaxPicWidth;
    UINT    MaxPicHeight;
} ENCODE_CAPS_VP9;
#else
// DDI v0.94
typedef struct tagENCODE_CAPS_VP9
{
    union {
        struct {
            UINT    CodingLimitSet : 1;
            UINT    Color420Only : 1;
            UINT    SegmentationSupport : 1;
            UINT    FrameLevelRateCtrl : 1;
            UINT    BRCReset : 1;
            UINT    MBBRCSupport : 1;
            UINT    TemporalLayerRateCtrl : 8;
            UINT    DynamicScaling : 1;
        UINT: 17;
        };
        UINT            CodingLimits;
    };
    union {
        struct {
            BYTE     EncodeFunc : 1;  // Enc+Pak
            BYTE     HybridPakFunc : 1;  // Hybrid Pak function
            BYTE    EncFunc : 1;  // Enc only function
        BYTE: 6;
        };
        BYTE CodingFunction;
    };
    UINT    MaxPicWidth;
    UINT    MaxPicHeight;

} ENCODE_CAPS_VP9;
#endif

    class DriverEncoder;

    mfxStatus QueryHwCaps(mfxCoreInterface * pCore, ENCODE_CAPS_VP9 & caps, GUID guid);

    DriverEncoder* CreatePlatformVp9Encoder(mfxCoreInterface * pCore);

    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual mfxStatus CreateAuxilliaryDevice(
                        mfxCoreInterface * pCore,
                        GUID               guid,
                        mfxU32             width,
                        mfxU32             height) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            VP9MfxParam const & par) = 0;

        virtual
        mfxStatus Reset(
            VP9MfxParam const & par) = 0;

        virtual
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) = 0;

        virtual
        mfxStatus Execute(
            Task const &task,
            mfxHDL surface) = 0;

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request,
            mfxU32 frameWidth,
            mfxU32 frameHeight) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP9 & caps) = 0;

        virtual
        mfxStatus QueryStatus(
            Task & task ) = 0;

        virtual
            mfxU32 GetReconSurfFourCC() = 0;

        virtual
        mfxStatus Destroy() = 0;
    };

#define VP9_MAX_UNCOMPRESSED_HEADER_SIZE 1000
#define SWITCHABLE 4 /* should be the last one */

    enum {
        LAST_FRAME   = 0,
        GOLDEN_FRAME = 1,
        ALTREF_FRAME = 2
    };

    struct BitOffsets
    {
        mfxU16 BitOffsetForLFRefDelta;
        mfxU16 BitOffsetForLFModeDelta;
        mfxU16 BitOffsetForLFLevel;
        mfxU16 BitOffsetForQIndex;
        mfxU16 BitOffsetForFirstPartitionSize;
        mfxU16 BitOffsetForSegmentation;
        mfxU16 BitSizeForSegmentation;
    };

    inline void AddSeqHeader(unsigned int width,
        unsigned int   height,
        unsigned int   FrameRateN,
        unsigned int   FrameRateD,
        unsigned int   numFramesInFile,
        unsigned char *pBitstream)
    {
        mfxU32   ivf_file_header[8] = { 0x46494B44, 0x00200000, 0x30395056, width + (height << 16), FrameRateN, FrameRateD, numFramesInFile, 0x00000000 };
        memcpy(pBitstream, ivf_file_header, sizeof (ivf_file_header));
    };

    inline void AddPictureHeader(unsigned char *pBitstream)
    {
        mfxU32 ivf_frame_header[3] = { 0x00000000, 0x00000000, 0x00000000 };
        memcpy(pBitstream, ivf_frame_header, sizeof (ivf_frame_header));
    };

    mfxStatus WriteUncompressedHeader(mfxU8 * buffer,
                                      mfxU32 bufferSizeBytes,
                                      Task const& task,
                                      VP9SeqLevelParam const &seqPar,
                                      BitOffsets &offsets,
                                      mfxU16 &bitsWritten);
} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
