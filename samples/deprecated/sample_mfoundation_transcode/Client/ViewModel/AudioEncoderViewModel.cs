#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class AudioEncoderViewModel : ViewModel {
        public AudioEncoderViewModel() {
            SelectedAudioFormat = AudioFormat.AAC;
            SamplesPerSecond = 44100;
            NumberOfChannels = 2;
            AverageBytesPerSecond = 16000;
        }

        private AudioFormat selectedAudioFormat;
        private uint samplesPerSecond;
        private uint numberOfChannels;
        private uint averageBytesPerSecond;
        private bool isEnabled;

        public AudioFormat SelectedAudioFormat {
            get { return selectedAudioFormat; }
            set {
                selectedAudioFormat = value;
                OnPropertyChanged(() => SelectedAudioFormat);
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

        public bool IsEnabled {
            get { return isEnabled; }
            set {
                isEnabled = value;
                OnPropertyChanged(() => IsEnabled);
            }
        }
    }
}