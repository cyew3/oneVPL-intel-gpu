#region Usings

using System.Windows;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class VideoDecoderViewModel : ViewModel {
        public VideoDecoderViewModel() {
            frameRate = new FrameRate(0, 0);
        }

        private string videoFormat;

        public string VideoFormat {
            get { return videoFormat; }
            set {
                videoFormat = value;
                OnPropertyChanged(() => VideoFormat);
            }
        }

        private readonly FrameRate frameRate;

        public uint FrameRateNumerator {
            get { return frameRate.Numerator; }
            set {
                frameRate.Numerator = value;
                OnPropertyChanged(() => FrameRateNumerator);
            }
        }

        public uint FrameRateDenominator {
            get { return frameRate.Denominator; }
            set {
                frameRate.Denominator = value;
                OnPropertyChanged(() => FrameRateDenominator);
            }
        }

        private Size frameSize;

        public int FrameSizeWidth {
            get { return (int) frameSize.Width; }
            set {
                frameSize.Width = value;
                OnPropertyChanged(() => FrameSizeWidth);
            }
        }

        public int FrameSizeHeight {
            get { return (int) frameSize.Height; }
            set {
                frameSize.Height = value;
                OnPropertyChanged(() => FrameSizeHeight);
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

        private string interlaceMode;

        public string InterlaceMode {
            get { return interlaceMode; }
            set {
                interlaceMode = value;
                OnPropertyChanged(() => InterlaceMode);
            }
        }
    }
}