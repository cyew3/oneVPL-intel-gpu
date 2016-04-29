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

class KnownException {
public:
    virtual ~KnownException() {}
};

class DecodeHeaderError : public KnownException{};
class DecodeInitError: public KnownException{};
class DecodeAllocError: public KnownException{};
class DecodeSetAllocatorError: public KnownException{};
class DecodeQueryIMPLError: public KnownException{};
class DecodeQueryIOSurfaceError: public KnownException{};
class DecodeQueryIOSizeError: public KnownException{};
class DecodeQueryError: public KnownException{};
class DecodeGetVideoParamError: public KnownException{};
class DecodeTimeoutError: public KnownException{};

class VideoSessionExtSetHandleError: public KnownException{};
class VideoSessionExtHWDeviceInitError: public KnownException{};
class VideoSessionExtAllocatorInitError: public KnownException{};
class VideoSessionExtSetFrameAllocatorError: public KnownException{};

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