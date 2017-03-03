#region Usings

using System.Collections.Generic;
using System.Linq;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class MediaSource {
        private readonly string sourceFileName;

        public MediaSource(string sourceFileName) {
            this.sourceFileName = sourceFileName;
            SourceResolver = SourceResolver.Create(sourceFileName);
            PresentationDescriptor = new PresentationDescriptor(Com.Get((out IMFPresentationDescriptor _) =>
                                                                        MFMediaSource.CreatePresentationDescriptor(out _)));
        }

        public PresentationDescriptor PresentationDescriptor { get; private set; }

        private List<SourceStreamNode> nodes;

        public IEnumerable<SourceStreamNode> Nodes {
            get {
                return nodes ??
                       (nodes = new List<SourceStreamNode>(
                                    from streamDescriptor
                                        in PresentationDescriptor.StreamDescriptors
                                    select new SourceStreamNode(this, streamDescriptor, streamDescriptor.MediaTypeHandler.CurrentMediaType.MajorType)));
            }
        }

        protected SourceResolver SourceResolver { get; private set; }

        private IMFMediaSource mfMediaSource;

        internal IMFMediaSource MFMediaSource {
            get {
                return mfMediaSource ??
                       (mfMediaSource = SourceResolver.GetMFMediaSource(sourceFileName));
            }
        }

        public ContainerFormat ContainerFormat {
            get {
                return (from streamDescriptor
                            in PresentationDescriptor.StreamDescriptors
                        select streamDescriptor.MediaTypeHandler
                        into mediaTypeHandler
                        from mediaType
                            in mediaTypeHandler.MediaTypes
                        where mediaType.GetContainerFormat(PresentationDescriptor) != ContainerFormat.Undefined
                        select mediaType.GetContainerFormat(PresentationDescriptor))
                    .FirstOrDefault();
            }
        }

        public VideoMediaType VideoMediaType {
            get {
                return (from streamDescriptor
                            in PresentationDescriptor.StreamDescriptors
                        select streamDescriptor.MediaTypeHandler
                        into mediaTypeHandler
                        from mediaType
                            in mediaTypeHandler.MediaTypes
                        where mediaType is VideoMediaType
                        select mediaType as VideoMediaType)
                    .FirstOrDefault();
            }
        }

        public VideoFormat VideoFormat {
            get { return VideoMediaType.VideoFormat; }
        }

        public AudioMediaType AudioMediaType {
            get {
                return (from streamDescriptor in PresentationDescriptor.StreamDescriptors
                        select streamDescriptor.MediaTypeHandler
                        into mediaTypeHandler
                        from mediaType in mediaTypeHandler.MediaTypes
                        where mediaType is AudioMediaType && mediaType.IsSupported
                        select mediaType as AudioMediaType).FirstOrDefault()
                       ?? new NullAudioMediaType();
            }
        }

        public AudioFormat AudioFormat {
            get { return AudioMediaType.AudioFormat; }
        }

        public void Shutdown() {
            MFMediaSource.Shutdown();
        }
    }
}