#region Usings

using System;
using System.Text;
using System.Windows;
using System.Windows.Input;
using Intel.MediaSDK.Samples.WPFTranscode.Client.Model;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class MainViewModel : ViewModel {
        private readonly IContext context;

        public MainViewModel(IOpenFileDialog openFileDialog, ISaveFileDialog saveFileDialog, IContext context) {
            this.context = context;
            this.context.Error += OnError;
            facade = new Facade();
            TranscodeGraphViewModel = new TranscodeGraphViewModel(openFileDialog, saveFileDialog, context);
            TranscodeGraphViewModel.SourceViewModel.SourceFileSelected += OnSourceFileSelected;
            Transcode = new DelegateCommand(TranscodeCommand);
            IsTranscodeButtonEnabled = false;
            Log = "4. Click Transcode button";
            Log = "3. Select target file";
            Log = "2. Configure video and audio encoder";
            Log = "1. Select source file";
        }

        private void OnSourceFileSelected() {
            IsTranscodeButtonEnabled = true;
            Log = string.Format("Source file '{0}' is selected", TranscodeGraphViewModel.SourceViewModel.SourceFileFullPath);
        }

        private string targetFileName;

        public string TargetFileName {
            get { return targetFileName; }
            set {
                targetFileName = value;
                OnPropertyChanged(() => TargetFileName);
            }
        }

        private readonly StringBuilder log = new StringBuilder();
        private readonly Facade facade;
        private bool isTranscodeButtonEnabled;

        public TranscodeGraphViewModel TranscodeGraphViewModel { get; private set; }

        public string Log {
            get { return log.ToString(); }
            set {
                log.Insert(0, string.Format("{0} - {1}{2}", DateTime.Now, value, Environment.NewLine));
                OnPropertyChanged(() => Log);
            }
        }

        public bool IsTranscodeButtonEnabled {
            get { return isTranscodeButtonEnabled; }
            set {
                isTranscodeButtonEnabled = value;
                OnPropertyChanged(() => IsTranscodeButtonEnabled);
            }
        }

        public ICommand Transcode { get; private set; }

        private void TranscodeCommand() {
            try {
                Validate();
                using (var transcoder = new Transcoder(facade)) {
                    transcoder.InitializeSource(TranscodeGraphViewModel.SourceViewModel.SourceFileFullPath);
                    transcoder.InitializeEncoderPlugins(
                        new VideoEncoderPluginInit {
                            VideoFormat = TranscodeGraphViewModel.VideoEncoderViewModel.SelectedVideoFormat,
                            FrameRate = new FrameRate(TranscodeGraphViewModel.VideoDecoderViewModel.FrameRateNumerator, TranscodeGraphViewModel.VideoDecoderViewModel.FrameRateDenominator),
                            FrameSize = new Size(TranscodeGraphViewModel.VideoDecoderViewModel.FrameSizeWidth, TranscodeGraphViewModel.VideoDecoderViewModel.FrameSizeHeight),
                            AverageBitrate = TranscodeGraphViewModel.VideoEncoderViewModel.AverageBitrate,
                            InterlaceMode = (InterlaceMode) Enum.Parse(typeof (InterlaceMode), TranscodeGraphViewModel.VideoDecoderViewModel.InterlaceMode),
                        },
                        new AudioEncoderPluginInit {
                            AudioFormat = TranscodeGraphViewModel.AudioEncoderViewModel.SelectedAudioFormat,
                            SamplesPerSecond = TranscodeGraphViewModel.AudioEncoderViewModel.SamplesPerSecond,
                            NumberOfChannels = TranscodeGraphViewModel.AudioEncoderViewModel.NumberOfChannels,
                            AverageBytesPerSecond = TranscodeGraphViewModel.AudioEncoderViewModel.AverageBytesPerSecond,
                        });
                    transcoder.InitializeTarget(TranscodeGraphViewModel.TargetViewModel.TargetFileFullPath);
                    Log = "Transcode initialized";
                    transcoder.Run();
                    Log = "Transcode started";
                    do {} while (transcoder.MediaSession.State == MediaSessionState.Started);
                    Log = "Transcode finished";
                }
            }
            catch (Exception e) {
                Log = e.ToString();
            }
            finally {
                facade.Close();
                facade.Open();
            }
        }

        private void Validate() {
            if (TranscodeGraphViewModel.SourceViewModel.SourceFileFullPath == null) {
                throw new InvalidOperationException("Please select the Source file");
            }
        }

        private void OnError(Exception e) {
            Log = e.ToString();
        }
    }
}