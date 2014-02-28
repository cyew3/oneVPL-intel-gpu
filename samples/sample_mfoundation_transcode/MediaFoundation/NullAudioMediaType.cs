namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class NullAudioMediaType : AudioMediaType {
        internal NullAudioMediaType() : base(MFHelper.MFCreateMediaType()) {}

        public override AudioFormat AudioFormat {
            get { return AudioFormat.Undefined; }
        }

        public override uint SamplesPerSecond {
            get { return 0; }
        }

        public override uint NumberOfChannels {
            get { return 0; }
        }

        public override uint AverageBytesPerSecond {
            get { return 0; }
        }

        public override uint BitsPerSample {
            get { return 0; }
        }

        public override bool Compressed {
            get { return false; }
        }

        public override bool PreferWaveFormatEx {
            get { return false; }
        }

        public override bool FixedSizeSamples {
            get { return false; }
        }
    }
}