//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#pragma once

class KnownException {
public:
    virtual ~KnownException() {}
};

class DecodeHeaderError : public KnownException{};
class DecodeInitError: public KnownException{};
class DecodeAllocError: public KnownException{};
class DecodeSetHandleError: public KnownException{};
class DecodeSetAllocatorError: public KnownException{};
class DecodeQueryIMPLError: public KnownException{};
class DecodeQueryIOSurfaceError: public KnownException{};
class DecodeQueryIOSizeError: public KnownException{};
class DecodeQueryError: public KnownException{};
class DecodeGetVideoParamError: public KnownException{};
class DecodeTimeoutError: public KnownException{};
class DecodeHWDeviceInitError: public KnownException{};
class DecodeAllocatorInitError: public KnownException{};

class EncodeInitError: public KnownException{};
class EncodeBitstreamEnlargeError: public KnownException{};
class EncodeSyncOperationError: public KnownException{};
class EncodeSyncOperationTimeout: public KnownException{};
class EncodeFrameAsyncError: public KnownException{};
class DecodeFrameAsyncError: public KnownException{};
class AudioEncodeSyncOperationError : public KnownException{};
class EncodeTimeoutError: public KnownException{};
class EncodeGetVideoParamError: public KnownException{};
class EncodeQueryIOSurfError: public KnownException{};
class EncodeQueryIOSizeError: public KnownException{};
class EncodeQueryError: public KnownException{};


class VPPInitError: public KnownException{};
class VPPRunFrameVPPAsyncError: public KnownException{};
class VPPTimeoutError: public KnownException{};
class VPPQueryIOSurfError: public KnownException{};
class VPPGetFrameAllocatorError: public KnownException{};
class VPPAllocError: public KnownException{};

class NullPointerError: public KnownException{};

class NoFreeSampleError : public KnownException{};
class IncompatibleSampleTypeError: public KnownException{};
class SampleBitstreamNullDatalenError: public KnownException{};
class SampleAudioFrameNullDatalenError: public KnownException{};

class MuxerHeaderBufferOverflow: public KnownException{};

class SamplePoolLockInvalidSample: public KnownException{};

class SplitterInitError: public KnownException{};
class SplitterGetInfoError: public KnownException{};
class SplitterTrackUnsupported: public KnownException{};
class SplitterGetBitstreamError: public KnownException{};

class MuxerPutSampleError: public KnownException{};
class MuxerInitError: public KnownException{};

class AllocatorTypeNotSupportedError: public KnownException{};
class HWDeviceTypeNotSupportedError: public KnownException{};
class D3D11NotSupportedError: public KnownException{};

class FactoryGetHWHandleError: public KnownException{};

class MFXVideoSessionInitError: public KnownException{};
class MFXAudioSessionInitError: public KnownException{};
class CommandlineConsistencyError: public KnownException{};

//profile exceptions set
class PipelineProfileError: public KnownException{};
class PipelineProfileNoOuputError: public KnownException{};
class PipelineProfileOutputFormatError: public KnownException{};
class PipelineProfileOnlyAudioTranscodeError: public KnownException{};
class PipelineProfileOnlyVideoTranscodeError: public KnownException{};
class UnsupportedAudioCodecFromExtension: public KnownException{};
class UnsupportedVideoCodecFromExtension: public KnownException{};
class UnsupportedAudioCodecError: public KnownException{};
class UnsupportedVideoCodecError: public KnownException{};
class CannotGetExtensionError: public KnownException{};
class NothingToTranscode: public KnownException{};
class AudioBitrateError: public KnownException{};


class IncompatibleParamTypeError: public KnownException{};
class FileOpenError: public KnownException{};

//metasample exceptions
class GetSampleTypeUnsupported: public KnownException{};
class GetBitstreamUnsupported: public KnownException{};
class GetAudioFrameUnsupported: public KnownException{};
class GetSurfaceUnsupported: public KnownException{};


class UnknownLoggerLevelError: public KnownException{};