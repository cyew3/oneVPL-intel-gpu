#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class SourceStreamNode : Node {
        public SourceStreamNode(MediaSource mediaSource, StreamDescriptor streamDescriptor, MajorType majorType) : base(NodeType.SourceStream) {
            MediaSource = mediaSource;
            PresentationDescriptor = mediaSource.PresentationDescriptor;
            StreamDescriptor = streamDescriptor;
            MajorType = majorType;
        }

        public MediaSource MediaSource {
            set { MFNode.SetUnknown(new Guid(0x835c58ec, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6), value.MFMediaSource); }
        }

        public PresentationDescriptor PresentationDescriptor {
            set { MFNode.SetUnknown(new Guid(0x835c58ed, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6), value.MFPresentationDescriptor); }
        }

        public StreamDescriptor StreamDescriptor {
            set { MFNode.SetUnknown(new Guid(0x835c58ee, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6), value.MFStreamDescriptor); }
        }

        public MajorType MajorType {
            get { return new MajorType(Com.Get((Guid _, out Guid __) => MFNode.GetGUID(_, out __), new Guid(0x7E6C6B2D, 0x0240, 0x4BA1, 0xA2, 0x5B, 0xDF, 0x82, 0x2C, 0x44, 0x67, 0x13))); }
            set { MFNode.SetGUID(new Guid(0x7E6C6B2D, 0x0240, 0x4BA1, 0xA2, 0x5B, 0xDF, 0x82, 0x2C, 0x44, 0x67, 0x13), value.MFMajorType); }
        }
    }
}