#region Usings

using System;
using System.IO;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class TranscodeGraphViewModel : ViewModel {
        private readonly IContext context;

        public TranscodeGraphViewModel(IOpenFileDialog openFileDialog, ISaveFileDialog saveFileDialog, IContext context) {
            this.context = context;

            SourceViewModel = new SourceViewModel(openFileDialog);
            SourceViewModel.SourceFileSelected += OnSourceFileSelected;

            VideoDecoderViewModel = new VideoDecoderViewModel();
            AudioDecoderViewModel = new AudioDecoderViewModel();
            VideoEncoderViewModel = new VideoEncoderViewModel {IsEnabled = false};
            AudioEncoderViewModel = new AudioEncoderViewModel {IsEnabled = false};
            TargetViewModel = new TargetViewModel(saveFileDialog) {IsEnabled = false};
        }

        public SourceViewModel SourceViewModel { get; private set; }
        public VideoDecoderViewModel VideoDecoderViewModel { get; private set; }
        public AudioDecoderViewModel AudioDecoderViewModel { get; private set; }
        public VideoEncoderViewModel VideoEncoderViewModel { get; private set; }
        public AudioEncoderViewModel AudioEncoderViewModel { get; private set; }
        public TargetViewModel TargetViewModel { get; private set; }

        private void OnSourceFileSelected() {
            try {
                Clear();

                var mediaSource = new MediaSource(SourceViewModel.SourceFileFullPath);

                var videoMediaType = mediaSource.VideoMediaType;
                VideoDecoderViewModel.VideoFormat = videoMediaType.VideoFormat.ToString();
                VideoDecoderViewModel.FrameRateNumerator = videoMediaType.FrameRate.Numerator;
                VideoDecoderViewModel.FrameRateDenominator = videoMediaType.FrameRate.Denominator;
                VideoDecoderViewModel.FrameSizeWidth = (int) videoMediaType.FrameSize.Width;
                VideoDecoderViewModel.FrameSizeHeight = (int) videoMediaType.FrameSize.Height;
                VideoDecoderViewModel.AverageBitrate = videoMediaType.AverageBitrate;
                VideoDecoderViewModel.InterlaceMode = videoMediaType.InterlaceMode.ToString();

                VideoEncoderViewModel.AverageBitrate = VideoDecoderViewModel.AverageBitrate;

                var audioMediaType = mediaSource.AudioMediaType;
                AudioDecoderViewModel.AudioFormat = audioMediaType.AudioFormat.ToString();
                AudioDecoderViewModel.SamplesPerSecond = audioMediaType.SamplesPerSecond;
                AudioDecoderViewModel.NumberOfChannels = audioMediaType.NumberOfChannels;
                AudioDecoderViewModel.AverageBytesPerSecond = audioMediaType.AverageBytesPerSecond;

                var sourceFileFullPath = SourceViewModel.SourceFileFullPath;
                TargetViewModel.TargetFileFullPath = Path.Combine(
                    Path.GetDirectoryName(sourceFileFullPath) ?? string.Empty,
                    "Transcoded.mp4");

                VideoEncoderViewModel.IsEnabled = true;

                if (audioMediaType.AudioFormat != AudioFormat.Undefined) {
                    AudioEncoderViewModel.IsEnabled = true;
                }

                TargetViewModel.IsEnabled = true;
            }
            catch (Exception e) {
                context.ThrowError(e);
            }
        }

        private void Clear() {
                VideoDecoderViewModel.VideoFormat = string.Empty;
                VideoDecoderViewModel.FrameRateNumerator = 0;
                VideoDecoderViewModel.FrameRateDenominator = 0;
                VideoDecoderViewModel.FrameSizeWidth = 0;
                VideoDecoderViewModel.FrameSizeHeight = 0;
                VideoDecoderViewModel.AverageBitrate = 0;
                VideoDecoderViewModel.InterlaceMode = string.Empty;

                VideoEncoderViewModel.AverageBitrate = VideoDecoderViewModel.AverageBitrate;

                AudioDecoderViewModel.AudioFormat = string.Empty;
                AudioDecoderViewModel.SamplesPerSecond = 0;
                AudioDecoderViewModel.NumberOfChannels = 0;
                AudioDecoderViewModel.AverageBytesPerSecond = 0;
        }
    }
}