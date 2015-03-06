#region Usings

using System.Linq;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class SourcePlugin : Plugin {
        public SourcePlugin(MediaSource mediaSource) {
            MediaSource = mediaSource;
        }

        public MediaSource MediaSource { get; private set; }

        public SourceStreamNode AudioSourceStreamNode {
            get { return MediaSource.Nodes.SingleOrDefault(_ => _.MajorType == MajorType.Audio); }
        }

        public SourceStreamNode VideoSourceStreamNode {
            get { return MediaSource.Nodes.Single(_ => _.MajorType == MajorType.Video); }
        }
    }
}