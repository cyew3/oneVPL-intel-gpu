using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Classes;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class VideoEncodeTransform : Transform {
        public VideoEncodeTransform(VideoFormat videoFormat) {
            switch (videoFormat) {
                case VideoFormat.H264:
                    MFTransform = (IMFTransform) new MFVideoEncoderH264();
                    break;
                default:
                    throw new ArgumentException(string.Format("Unable to create a VideoEncodeTransform for video format {0}", videoFormat));
            }
        }
    }
}