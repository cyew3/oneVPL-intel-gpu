#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class VideoEncoderPlugin : Plugin {
        public VideoEncoderPlugin(VideoEncoderPluginInit init) {
            EncodeTransformNode = new EncodeTransformNode(
                videoFormat: init.VideoFormat,
                frameRate: init.FrameRate,
                frameSize: init.FrameSize,
                averageBitrate: init.AverageBitrate,
                interlaceMode: init.InterlaceMode,
                allSamplesIndependent: true);
        }

        public VideoMediaType MediaType {
            get { return (VideoMediaType) EncodeTransformNode.MediaType; }
        }

        public EncodeTransformNode EncodeTransformNode { get; private set; }
    }
}