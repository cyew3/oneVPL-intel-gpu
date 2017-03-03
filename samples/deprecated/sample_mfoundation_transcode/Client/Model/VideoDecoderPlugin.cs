#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class VideoDecoderPlugin : Plugin {
        public VideoDecoderPlugin(MediaSource mediaSource) {
            VideoFormat = mediaSource.VideoFormat;
            DecodeTransformNode = new DecodeTransformNode(VideoFormat);
        }

        public VideoFormat VideoFormat { get; private set; }

        public DecodeTransformNode DecodeTransformNode { get; private set; }
    }
}