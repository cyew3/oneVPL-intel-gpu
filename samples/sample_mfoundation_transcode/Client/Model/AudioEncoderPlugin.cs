#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class AudioEncoderPlugin : Plugin {
        public AudioEncoderPlugin(AudioEncoderPluginInit init) {
            EncodeTransformNode = new EncodeTransformNode(
                audioFormat: init.AudioFormat,
                samplesPerSecond: init.SamplesPerSecond,
                numberOfChannels: init.NumberOfChannels,
                averageBytesPerSecond: init.AverageBytesPerSecond,
                compressed: true,
                preferWaveFormatEx: true,
                fixedSizeSamples: false);
        }

        public AudioMediaType MediaType {
            get { return (AudioMediaType) EncodeTransformNode.MediaType; }
        }

        public EncodeTransformNode EncodeTransformNode { get; private set; }
    }
}