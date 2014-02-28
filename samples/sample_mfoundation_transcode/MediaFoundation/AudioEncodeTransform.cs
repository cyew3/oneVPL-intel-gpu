#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Classes;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class AudioEncodeTransform : Transform {
        public AudioEncodeTransform(AudioFormat audioFormat) {
            switch (audioFormat) {
                case AudioFormat.AAC:
                    MFTransform = (IMFTransform) new MFAudioEncoderAAC();
                    break;
                default:
                    throw new ArgumentException(string.Format("Unable to create an AndioEncodeTransform for audio format {0}", audioFormat));
            }
        }
    }
}