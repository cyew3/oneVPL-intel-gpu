#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class TargetPlugin : Plugin {
        public TargetPlugin(string fileName, AudioEncoderPlugin audioEncoderPlugin, VideoEncoderPlugin videoEncoderPlugin) {
            var byteStream = new ByteStream(fileName);
            var mediaSink = new Mpeg4MediaSink(byteStream, audioEncoderPlugin == null ? null : audioEncoderPlugin.MediaType, videoEncoderPlugin.MediaType);
            const int videoStreamIndex = 0;
            const int audioStreamIndex = 1;
            if (audioEncoderPlugin != null && audioEncoderPlugin.MediaType != null) AudioOutputNode = new AudioOutputNode(mediaSink, audioStreamIndex);
            if (videoEncoderPlugin.MediaType != null) VideoOutputNode = new VideoOutputNode(mediaSink, videoStreamIndex);
        }

        public AudioOutputNode AudioOutputNode { get; private set; }
        public VideoOutputNode VideoOutputNode { get; private set; }
    }
}