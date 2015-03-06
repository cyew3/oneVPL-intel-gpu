#region Usings

using System;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Classes;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class DecodeTransform : Transform {
        internal DecodeTransform(AudioFormat audioFormat) {
            switch (audioFormat) {
                case AudioFormat.AAC:
                    MFTransform = (IMFTransform) new MFAudioDecoderAAC();
                    break;
                case AudioFormat.WMA:
                    MFTransform = (IMFTransform) new MFAudioDecoderWMA();
                    break;
                case AudioFormat.MP3:
                    MFTransform = (IMFTransform) new MFAudioDecoderMP3();
                    break;
                default:
                    throw new ArgumentException(string.Format("Unable to create a DecodeTransform for audio format {0}", audioFormat));
            }
        }

        public DecodeTransform(VideoFormat videoFormat) {
            switch (videoFormat) {
                case VideoFormat.H264:
                    MFTransform = CreateH264VideoTransform();
                    break;
                default:
                    throw new ArgumentException(string.Format("Unable to create a DecodeTransform for video format {0}", videoFormat));
            }
        }

        private static IMFTransform CreateH264VideoTransform() {
            return TryCreate(() => (IMFTransform) new MFVideoDecoderH264Intel()) ?? 
                   TryCreate(() => (IMFTransform) new MFVideoDecoderH264Microsoft());
        }

        private static IMFTransform TryCreate(Func<IMFTransform> create) {
            try {
                return create();
            }
            catch (COMException) {}
            return null;
        }
    }
}