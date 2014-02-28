#region Usings

using System.Windows;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class VideoEncoderPluginInit {
        public VideoEncoderPluginInit() {
            VideoFormat = VideoFormat.H264;
            FrameRate = new FrameRate(numerator: 25, denominator: 1);
            FrameSize = new Size(width: 352, height: 288);
            AverageBitrate = 921600;
            InterlaceMode = InterlaceMode.Progressive;
        }

        public VideoFormat VideoFormat { get; set; }
        public FrameRate FrameRate { get; set; }
        public Size FrameSize { get; set; }
        public uint AverageBitrate { get; set; }
        public InterlaceMode InterlaceMode { get; set; }
    }
}