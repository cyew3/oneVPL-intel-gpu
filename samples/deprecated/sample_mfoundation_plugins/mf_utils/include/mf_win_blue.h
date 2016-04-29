/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once
#ifndef STATIC_CODECAPI_AVEncVideoLTRBufferControl

    #define STATIC_CODECAPI_AVEncVideoEncodeFrameTypeQP 0xaa70b610, 0xe03f, 0x450c, 0xad, 0x07, 0x07, 0x31, 0x4e, 0x63, 0x9c, 0xe7
    #define STATIC_CODECAPI_AVEncSliceControlMode       0xe9e782ef, 0x5f18, 0x44c9, 0xa9, 0x0b, 0xe9, 0xc3, 0xc2, 0xc1, 0x7b, 0x0b
    #define STATIC_CODECAPI_AVEncSliceControlSize       0x92f51df3, 0x07a5, 0x4172, 0xae, 0xfe, 0xc6, 0x9c, 0xa3, 0xb6, 0x0e, 0x35
    #define STATIC_CODECAPI_AVEncVideoMaxNumRefFrame    0x964829ed, 0x94f9, 0x43b4, 0xb7, 0x4d, 0xef, 0x40, 0x94, 0x4b, 0x69, 0xa0
    #define STATIC_CODECAPI_AVEncVideoMeanAbsoluteDifference    0xe5c0c10f, 0x81a4, 0x422d, 0x8c, 0x3f, 0xb4, 0x74, 0xa4, 0x58, 0x13, 0x36
    #define STATIC_CODECAPI_AVEncVideoMaxQP             0x3daf6f66, 0xa6a7, 0x45e0, 0xa8, 0xe5, 0xf2, 0x74, 0x3f, 0x46, 0xa3, 0xa2
    #define STATIC_CODECAPI_AVEncVideoLTRBufferControl  0xa4a0e93d, 0x4cbc, 0x444c, 0x89, 0xf4, 0x82, 0x6d, 0x31, 0x0e, 0x92, 0xa7
    #define STATIC_CODECAPI_AVEncVideoMarkLTRFrame      0xe42f4748, 0xa06d, 0x4ef9, 0x8c, 0xea, 0x3d, 0x05, 0xfd, 0xe3, 0xbd, 0x3b
    #define STATIC_CODECAPI_AVEncVideoUseLTRFrame       0x00752db8, 0x55f7, 0x4f80, 0x89, 0x5b, 0x27, 0x63, 0x91, 0x95, 0xf2, 0xad
    #define STATIC_CODECAPI_AVEncVideoUseAndMarkLTRFrame    0xe1a89844, 0xd3c7, 0x45a9, 0xb5, 0xd8, 0xd1, 0xeb, 0xff, 0xcc, 0xb7, 0xbe

// AVEncVideoEncodeFrameTypeQP (UINT64)
DEFINE_CODECAPI_GUID( AVEncVideoEncodeFrameTypeQP, "aa70b610-e03f-450c-ad07-07314e639ce7", 0xaa70b610, 0xe03f, 0x450c, 0xad, 0x07, 0x07, 0x31, 0x4e, 0x63, 0x9c, 0xe7 )

// AVEncSliceControlMode (UINT32)
DEFINE_CODECAPI_GUID( AVEncSliceControlMode, "e9e782ef-5f18-44c9-a90b-e9c3c2c17b0b", 0xe9e782ef, 0x5f18, 0x44c9, 0xa9, 0x0b, 0xe9, 0xc3, 0xc2, 0xc1, 0x7b, 0x0b )

// AVEncSliceControlSize (UINT32)
DEFINE_CODECAPI_GUID( AVEncSliceControlSize, "92f51df3-07a5-4172-aefe-c69ca3b60e35", 0x92f51df3, 0x07a5, 0x4172, 0xae, 0xfe, 0xc6, 0x9c, 0xa3, 0xb6, 0x0e, 0x35 )

// AVEncVideoMaxNumRefFrame (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoMaxNumRefFrame, "964829ed-94f9-43b4-b74d-ef40944b69a0", 0x964829ed, 0x94f9, 0x43b4, 0xb7, 0x4d, 0xef, 0x40, 0x94, 0x4b, 0x69, 0xa0 )

// AVEncVideoMeanAbsoluteDifference (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoMeanAbsoluteDifference, "e5c0c10f-81a4-422d-8c3f-b474a4581336", 0xe5c0c10f, 0x81a4, 0x422d, 0x8c, 0x3f, 0xb4, 0x74, 0xa4, 0x58, 0x13, 0x36 )

// AVEncVideoMaxQP (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoMaxQP, "3daf6f66-a6a7-45e0-a8e5-f2743f46a3a2", 0x3daf6f66, 0xa6a7, 0x45e0, 0xa8, 0xe5, 0xf2, 0x74, 0x3f, 0x46, 0xa3, 0xa2 )

// AVEncVideoLTRBufferControl (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoLTRBufferControl, "a4a0e93d-4cbc-444c-89f4-826d310e92a7", 0xa4a0e93d, 0x4cbc, 0x444c, 0x89, 0xf4, 0x82, 0x6d, 0x31, 0x0e, 0x92, 0xa7 )

// AVEncVideoMarkLTRFrame (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoMarkLTRFrame, "e42f4748-a06d-4ef9-8cea-3d05fde3bd3b", 0xe42f4748, 0xa06d, 0x4ef9, 0x8c, 0xea, 0x3d, 0x05, 0xfd, 0xe3, 0xbd, 0x3b )

// AVEncVideoUseLTRFrame (UINT32)
DEFINE_CODECAPI_GUID( AVEncVideoUseLTRFrame, "00752db8-55f7-4f80-895b-27639195f2ad", 0x00752db8, 0x55f7, 0x4f80, 0x89, 0x5b, 0x27, 0x63, 0x91, 0x95, 0xf2, 0xad )

// AVEncVideoUseAndMarkLTRFrame (UINT64)
DEFINE_CODECAPI_GUID( AVEncVideoUseAndMarkLTRFrame, "e1a89844-d3c7-45a9-b5d8-d1ebffccb7be", 0xe1a89844, 0xd3c7, 0x45a9, 0xb5, 0xd8, 0xd1, 0xeb, 0xff, 0xcc, 0xb7, 0xbe )

    #define CODECAPI_AVEncVideoEncodeFrameTypeQP DEFINE_CODECAPI_GUIDNAMED( AVEncVideoEncodeFrameTypeQP )
    #define CODECAPI_AVEncSliceControlMode       DEFINE_CODECAPI_GUIDNAMED( AVEncSliceControlMode )
    #define CODECAPI_AVEncSliceControlSize       DEFINE_CODECAPI_GUIDNAMED( AVEncSliceControlSize )
    #define CODECAPI_AVEncVideoMaxNumRefFrame    DEFINE_CODECAPI_GUIDNAMED( AVEncVideoMaxNumRefFrame )
    #define CODECAPI_AVEncVideoMeanAbsoluteDifference    DEFINE_CODECAPI_GUIDNAMED( AVEncVideoMeanAbsoluteDifference )
    #define CODECAPI_AVEncVideoMaxQP             DEFINE_CODECAPI_GUIDNAMED( AVEncVideoMaxQP )
    #define CODECAPI_AVEncVideoLTRBufferControl  DEFINE_CODECAPI_GUIDNAMED( AVEncVideoLTRBufferControl )
    #define CODECAPI_AVEncVideoMarkLTRFrame      DEFINE_CODECAPI_GUIDNAMED( AVEncVideoMarkLTRFrame )
    #define CODECAPI_AVEncVideoUseLTRFrame       DEFINE_CODECAPI_GUIDNAMED( AVEncVideoUseLTRFrame )
    #define CODECAPI_AVEncVideoUseAndMarkLTRFrame DEFINE_CODECAPI_GUIDNAMED( AVEncVideoUseAndMarkLTRFrame )

// MFSampleExtension_MeanAbsoluteDifference {1cdbde11-08b4-4311-a6dd-0f9f371907aa}
// Type: UINT32
DEFINE_GUID(MFSampleExtension_MeanAbsoluteDifference,
0x1cdbde11, 0x08b4, 0x4311, 0xa6, 0xdd, 0x0f, 0x9f, 0x37, 0x19, 0x07, 0xaa);

// MFSampleExtension_LongTermReferenceFrameInfo {9154733f-e1bd-41bf-81d3-fcd918f71332}
// Type: UINT32
DEFINE_GUID(MFSampleExtension_LongTermReferenceFrameInfo,
0x9154733f, 0xe1bd, 0x41bf, 0x81, 0xd3, 0xfc, 0xd9, 0x18, 0xf7, 0x13, 0x32);

// MFSampleExtension_ROIRectangle {3414a438-4998-4d2c-be82-be3ca0b24d43}
// Type: BLOB
DEFINE_GUID(MFSampleExtension_ROIRectangle,
0x3414a438, 0x4998, 0x4d2c, 0xbe, 0x82, 0xbe, 0x3c, 0xa0, 0xb2, 0x4d, 0x43);

typedef struct _ROI_AREA {
  RECT rect;
  INT32 QPDelta;
} ROI_AREA, *PROI_AREA;

DEFINE_GUID( MFVideoFormat_HEVC, 0x68766331, 0x767A, 0x494D, 0xB4, 0x78, 0xF2, 0x9D, 0x25, 0xDC, 0x90, 0x37);
DEFINE_MEDIATYPE_GUID( MFVideoFormat_HEVC_ES,   FCC('HEVS') );

// VIDEO - Generic compressed video extra data
//

// {ad76a80b-2d5c-4e0b-b375-64e520137036}   MF_MT_VIDEO_PROFILE             {UINT32}    This is an alias of  MF_MT_MPEG2_PROFILE
DEFINE_GUID(MF_MT_VIDEO_PROFILE,
0xad76a80b, 0x2d5c, 0x4e0b, 0xb3, 0x75, 0x64, 0xe5, 0x20, 0x13, 0x70, 0x36);

// {96f66574-11c5-4015-8666-bff516436da7}   MF_MT_VIDEO_LEVEL               {UINT32}    This is an alias of  MF_MT_MPEG2_LEVEL
DEFINE_GUID(MF_MT_VIDEO_LEVEL,
0x96f66574, 0x11c5, 0x4015, 0x86, 0x66, 0xbf, 0xf5, 0x16, 0x43, 0x6d, 0xa7);

//

#define eAVEncH264VLevel5_2 52


// {E3F2E203-D445-4B8C-9211-AE390D3BA017}  {UINT32} Maximum macroblocks per second that can be handled by MFT
DEFINE_GUID(MF_VIDEO_MAX_MB_PER_SEC,
0xe3f2e203, 0xd445, 0x4b8c, 0x92, 0x11, 0xae, 0x39, 0xd, 0x3b, 0xa0, 0x17);

//    MF_VIDEO_MAX_MB_PER_SEC
//
//    Data type : UINT32
//
//    MF_VIDEO_MAX_MB_PER_SEC on IMFTransform specifies the maximum macroblock processing rate (in unit of macroblocks per second) supported by the hardware encoder. The value shall be independent of how the pipeline is constructed. This is read only attribute, and we expect encoder to return static value based on the hardware capability. Hardware shall return the lower bound on this value. For example if hardware has lower processing power when operating in low latency mode, it should return the lower value.
//    Only lower 28 bits should be used by app. Upper 4bits are reserved for future use. Applications should ignore the upper 4 bits and MFT should set the upper 4 bits to 0.
//
//    Applicable to H.264/AVC encoders.
//    Sample code
//    CHECKHR_GOTO_DONE (pMFT->GetAttributes(&pAttributes));
//    hr = pAttributes->GetUINT32(MF_VIDEO_MAX_MB_PER_SEC, &uiMaxMBPerSec);
//    if (SUCCEEDED(hr)) { // uiMaxMBPerSec&0x0fffffff contains valid value }

#endif