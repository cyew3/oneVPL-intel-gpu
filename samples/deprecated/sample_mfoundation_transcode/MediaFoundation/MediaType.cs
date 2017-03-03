#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public abstract class MediaType {
        private static readonly Guid MF_MT_MPEG4_SAMPLE_DESCRIPTION = new Guid(0x261e9d83, 0x9529, 0x4b8f, 0xa1, 0x11, 0x8b, 0x9c, 0x95, 0x0a, 0x81, 0xa9);

        private static readonly Guid MF_MT_MAJOR_TYPE = new Guid("48eba18e-f8c9-4687-bf11-0a74c9f96a8f");

        internal MediaType(IMFMediaType mfMediaType) {
            MFMediaType = mfMediaType;
        }

        internal static MediaType Create(IMFMediaType mfMediaType) {
            var majorType = Com.Get((out Guid _) => mfMediaType.GetMajorType(out _));
            if (majorType == MajorType.Audio.MFMajorType) {
                return new AudioMediaType(mfMediaType);
            }
            if (majorType == MajorType.Video.MFMajorType) {
                return new VideoMediaType(mfMediaType);
            }
            throw new ArgumentException("Invalid media type");
        }

        internal IMFMediaType MFMediaType { get; private set; }

        public ContainerFormat GetContainerFormat(PresentationDescriptor presentationDescriptor) {
            if (Com.NoExceptions(
                () => {
                    uint result;
                    MFMediaType.GetBlobSize(MF_MT_MPEG4_SAMPLE_DESCRIPTION, out result);
                })) {
                return ContainerFormat.MP4;
            }
            if (Com.NoExceptions(
                () => { var asfDataLength = presentationDescriptor.AsfDataLength; })) {
                return ContainerFormat.Mpeg2;
            }
            return ContainerFormat.Undefined;
        }

        protected internal Guid SubType {
            get {
                return Com.Get((Guid _, out Guid __) =>
                               MFMediaType.GetGUID(_, out __), Consts.MF_MT_SUBTYPE);
            }
            set { MFMediaType.SetGUID(Consts.MF_MT_SUBTYPE, value); }
        }

        private MajorType majorType;

        public MajorType MajorType {
            get { return majorType; }
            set {
                majorType = value;
                MFMediaType.SetGUID(MF_MT_MAJOR_TYPE, value.MFMajorType);
            }
        }

        public abstract bool IsSupported { get; }
    }
}