#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.View {
    public class ComboBoxItemAudioFormat {
        public AudioFormat AudioFormat { get; set; }
        public string DisplayValue { get; set; }
    }

    public class ComboBoxItemSamplesPerSecond {
        public uint SamplesPerSecond { get; set; }
        public string DisplayValue { get; set; }
    }

    public class ComboBoxItemNumberOfChannels {
        public uint NumberOfChannels { get; set; }
        public string DisplayValue { get; set; }
    }

    public class ComboBoxItemAverageBytesPerSecond {
        public uint AverageBytesPerSecond { get; set; }
        public string DisplayValue { get; set; }
    }
}