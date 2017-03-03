namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class AudioDecoderViewModel : ViewModel {
        private string audioFormat;
        private uint samplesPerSecond;
        private uint numberOfChannels;
        private uint averageBytesPerSecond;

        public string AudioFormat {
            get { return audioFormat; }
            set {
                audioFormat = value;
                OnPropertyChanged(() => AudioFormat);
            }
        }

        public uint SamplesPerSecond {
            get { return samplesPerSecond; }
            set {
                samplesPerSecond = value;
                OnPropertyChanged(() => SamplesPerSecond);
            }
        }

        public uint NumberOfChannels {
            get { return numberOfChannels; }
            set {
                numberOfChannels = value;
                OnPropertyChanged(() => NumberOfChannels);
            }
        }

        public uint AverageBytesPerSecond {
            get { return averageBytesPerSecond; }
            set {
                averageBytesPerSecond = value;
                OnPropertyChanged(() => AverageBytesPerSecond);
            }
        }
    }
}