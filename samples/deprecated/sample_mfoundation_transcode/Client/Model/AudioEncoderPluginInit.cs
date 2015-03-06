#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class AudioEncoderPluginInit {
        public AudioEncoderPluginInit() {
            AudioFormat = AudioFormat.AAC;
            SamplesPerSecond = 44100;
            NumberOfChannels = 2;
            AverageBytesPerSecond = 16000;
        }

        public AudioFormat AudioFormat { get; set; }
        public uint SamplesPerSecond { get; set; }
        public uint NumberOfChannels { get; set; }
        public uint AverageBytesPerSecond { get; set; }
    }
}