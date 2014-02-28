#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class VideoEncoderViewModel : ViewModel {
        public VideoEncoderViewModel() {
            SelectedVideoFormat = VideoFormat.H264;
        }

        private VideoFormat selectedVideoFormat;

        public VideoFormat SelectedVideoFormat {
            get { return selectedVideoFormat; }
            set {
                selectedVideoFormat = value;
                OnPropertyChanged(() => SelectedVideoFormat);
            }
        }

        private uint averageBitrate;

        public uint AverageBitrate {
            get { return averageBitrate; }
            set {
                averageBitrate = value;
                OnPropertyChanged(() => AverageBitrate);
            }
        }

        private bool isEnabled;

        public bool IsEnabled {
            get { return isEnabled; }
            set {
                isEnabled = value;
                OnPropertyChanged(() => IsEnabled);
            }
        }
    }
}