#region Usings

using System;
using System.Windows;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class VideoMediaType : MediaType {
        private static readonly Guid MFVideoFormat_H264 = new Guid("34363248-0000-0010-8000-00AA00389B71");
        private static readonly Guid MFVideoFormat_HEVC = new Guid("{68766331-767a-494d-b478-f29d25dc9037}");

        private static readonly Guid MF_MT_FRAME_RATE = new Guid("c459a2e8-3d2c-4e44-b132-fee5156c7bb0");
        private static readonly Guid MF_MT_FRAME_SIZE = new Guid("1652c33d-d6b2-4012-b834-72030849a37d");
        private static readonly Guid MF_MT_AVG_BITRATE = new Guid("20332624-fb0d-4d9e-bd0d-cbf6786c102e");
        private static readonly Guid MF_MT_INTERLACE_MODE = new Guid("e2724bb8-e676-4806-b4b2-a8d6efb44ccd");
        private static readonly Guid MF_MT_ALL_SAMPLES_INDEPENDENT = new Guid("c9173739-5e56-461c-b713-46fb995cb95f");

        internal VideoMediaType(IMFMediaType mfMediaType) : base(mfMediaType) {
            MajorType = MajorType.Video;
        }

        public VideoFormat VideoFormat {
            get {
                var subType = SubType;
                if (subType == MFVideoFormat_H264) {
                    return VideoFormat.H264;
                }
                if (subType == MFVideoFormat_HEVC) {
                    return VideoFormat.HEVC;
                }
                return VideoFormat.Undefined;
            }
            set {
                switch (value) {
                    case VideoFormat.H264:
                        SubType = MFVideoFormat_H264;
                        break;
                }
            }
        }

        public FrameRate FrameRate {
            get {
                var packedFrameRate = Com.Get((Guid _, out ulong __) => MFMediaType.GetUINT64(_, out __), MF_MT_FRAME_RATE);
                return new FrameRate(Left(packedFrameRate), Right(packedFrameRate));
            }
            set { MFMediaType.SetUINT64(MF_MT_FRAME_RATE, Pack(value.Numerator, value.Denominator)); }
        }

        public Size FrameSize {
            get {
                var packedFrameSize = Com.Get((Guid _, out ulong __) => MFMediaType.GetUINT64(_, out __), MF_MT_FRAME_SIZE);
                return new Size(Left(packedFrameSize), Right(packedFrameSize));
            }
            set { MFMediaType.SetUINT64(MF_MT_FRAME_SIZE, Pack((uint) value.Width, (uint) value.Height)); }
        }

        public uint AverageBitrate {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_AVG_BITRATE); }
            set { MFMediaType.SetUINT32(MF_MT_AVG_BITRATE, value); }
        }

        public InterlaceMode InterlaceMode {
            get { return (InterlaceMode) Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_INTERLACE_MODE); }
            set { MFMediaType.SetUINT32(MF_MT_INTERLACE_MODE, (uint) value); }
        }

        public bool AllSamplesIndependent {
            get { return Com.Get((Guid _, out uint __) => MFMediaType.GetUINT32(_, out __), MF_MT_ALL_SAMPLES_INDEPENDENT) == 1; }
            set { MFMediaType.SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, value.ToUint()); }
        }

        private static ulong Pack(uint left, uint right) {
            return (ulong) left << 32 | right;
        }

        private uint Left(ulong value) {
            return (uint) (value >> 32);
        }

        private uint Right(ulong value) {
            return (uint) (value & 0xFFFFFFFF);
        }

        public override bool IsSupported {
            get {
                return
                    VideoFormat == VideoFormat.H264;
            }
        }
    }
}