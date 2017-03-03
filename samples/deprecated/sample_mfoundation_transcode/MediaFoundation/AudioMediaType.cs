#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class AudioMediaType : MediaType {
        private static readonly Guid MFAudioFormat_PCM = new Guid("{00000001-0000-0010-8000-00AA00389B71}");
        private static readonly Guid MFAudioFormat_AAC = new Guid(0x1610, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
        private static readonly Guid MFAudioFormat_WMA = new Guid("00000161-0000-0010-8000-00aa00389b71");
        private static readonly Guid MFAudioFormat_MPEG = new Guid("00000050-0000-0010-8000-00aa00389b71");
        private static readonly Guid MFAudioFormat_MP3 = new Guid("00000055-0000-0010-8000-00aa00389b71");

        private static readonly Guid MF_MT_AUDIO_SAMPLES_PER_SECOND = new Guid("5faeeae7-0290-4c31-9e8a-c534f68d9dba");
        private static readonly Guid MF_MT_AUDIO_NUM_CHANNELS = new Guid("37e48bf5-645e-4c5b-89de-ada9e29b696a");
        private static readonly Guid MF_MT_AUDIO_AVG_BYTES_PER_SECOND = new Guid("1aab75c8-cfef-451c-ab95-ac034b8e1731");
        private static readonly Guid MF_MT_AUDIO_BITS_PER_SAMPLE = new Guid("f2deb57f-40fa-4764-aa33-ed4f2d1ff669");
        private static readonly Guid MF_MT_COMPRESSED = new Guid("3afd0cee-18f2-4ba5-a110-8bea502e1f92");
        private static readonly Guid MF_MT_AUDIO_PREFER_WAVEFORMATEX = new Guid("a901aaba-e037-458a-bdf6-545be2074042");
        private static readonly Guid MF_MT_FIXED_SIZE_SAMPLES = new Guid("b8ebefaf-b718-4e04-b0a9-116775e3321b");

        internal AudioMediaType(IMFMediaType mfMediaType) : base(mfMediaType) {
            MajorType = MajorType.Audio;
        }

        public virtual AudioFormat AudioFormat {
            get {
                var subType = SubType;
                if (subType == MFAudioFormat_AAC) {
                    return AudioFormat.AAC;
                }
                if (subType == MFAudioFormat_PCM) {
                    return AudioFormat.PCM;
                }
                if (subType == MFAudioFormat_WMA) {
                    return AudioFormat.WMA;
                }
                if (subType == MFAudioFormat_MPEG) {
                    return AudioFormat.Mpeg;
                }
                if (subType == MFAudioFormat_MP3) {
                    return AudioFormat.MP3;
                }
                return AudioFormat.Undefined;
            }
            set {
                switch (value) {
                    case AudioFormat.AAC:
                        SubType = MFAudioFormat_AAC;
                        break;
                    case AudioFormat.PCM:
                        SubType = MFAudioFormat_PCM;
                        break;
                }
            }
        }

        public virtual uint SamplesPerSecond {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AUDIO_SAMPLES_PER_SECOND); }
            set { MFMediaType.SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, value); }
        }

        public virtual uint NumberOfChannels {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AUDIO_NUM_CHANNELS); }
            set { MFMediaType.SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, value); }
        }

        public virtual uint AverageBytesPerSecond {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AUDIO_AVG_BYTES_PER_SECOND); }
            set { MFMediaType.SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, value); }
        }

        public virtual uint BitsPerSample {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AUDIO_BITS_PER_SAMPLE); }
            set { MFMediaType.SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, value); }
        }

        public virtual bool Compressed {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_COMPRESSED) == 1; }
            set { MFMediaType.SetUINT32(MF_MT_COMPRESSED, value.ToUint()); }
        }

        public virtual bool PreferWaveFormatEx {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AUDIO_PREFER_WAVEFORMATEX) == 1; }
            set { MFMediaType.SetUINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, value.ToUint()); }
        }

        public virtual bool FixedSizeSamples {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_FIXED_SIZE_SAMPLES) == 1; }
            set { MFMediaType.SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, value.ToUint()); }
        }

        public override bool IsSupported {
            get {
                return
                    AudioFormat == AudioFormat.AAC ||
                    AudioFormat == AudioFormat.PCM ||
                    AudioFormat == AudioFormat.MP3;
            }
        }
    }
}