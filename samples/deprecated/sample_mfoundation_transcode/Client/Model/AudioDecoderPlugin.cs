#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class AudioDecoderPlugin : Plugin {
        public AudioDecoderPlugin(MediaSource mediaSource) {
            AudioFormat = mediaSource.AudioFormat;
            DecodeTransformNode = new DecodeTransformNode(AudioFormat);
        }

        public AudioFormat AudioFormat { get; private set; }

        public DecodeTransformNode DecodeTransformNode { get; private set; }
    }
}