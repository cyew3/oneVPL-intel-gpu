#region Usings

using System;
using System.Runtime.InteropServices;
using System.Windows;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class EncodeTransformNode : TransformNode {
        public EncodeTransformNode(
            AudioFormat audioFormat,
            uint samplesPerSecond,
            uint numberOfChannels,
            uint averageBytesPerSecond,
            bool compressed = true,
            bool preferWaveFormatEx = true,
            bool fixedSizeSamples = false
            ) : base(new AudioEncodeTransform(audioFormat)) {
            MediaType = new AudioMediaType(MFHelper.MFCreateMediaType()) {
                AudioFormat = audioFormat,
                SamplesPerSecond = samplesPerSecond,
                NumberOfChannels = numberOfChannels,
                AverageBytesPerSecond = averageBytesPerSecond,
                Compressed = compressed,
                PreferWaveFormatEx = preferWaveFormatEx,
                FixedSizeSamples = fixedSizeSamples,
                BitsPerSample = 16
            };
            OutputType = MediaType;
        }

        public EncodeTransformNode(
            VideoFormat videoFormat,
            FrameRate frameRate,
            Size frameSize,
            uint averageBitrate,
            InterlaceMode interlaceMode,
            bool allSamplesIndependent
            ) : base(new VideoEncodeTransform(videoFormat)) {
            try {
                MediaType = new VideoMediaType(MFHelper.MFCreateMediaType()) {
                    VideoFormat = videoFormat,
                    FrameRate = frameRate,
                    FrameSize = frameSize,
                    AverageBitrate = averageBitrate,
                    InterlaceMode = interlaceMode,
                    AllSamplesIndependent = allSamplesIndependent
                };
                OutputType = MediaType;
            }
            catch (COMException e) {
                throw new ApplicationException("Unable to initialize Intel H.264 Hardware Video Encoder.", e);
            }
        }

        private MediaType OutputType {
            set { Transform.MFTransform.SetOutputType(0, value.MFMediaType, MFTSetTypeFlags.None); }
        }
    }
}