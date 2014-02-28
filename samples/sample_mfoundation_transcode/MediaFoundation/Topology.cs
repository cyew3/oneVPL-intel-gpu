#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class Topology {
        public Topology() {
            MFTopology = MFHelper.MFCreateTopology();
        }

        public void Add(Node node) {
            MFTopology.Add(node.MFNode);
        }

        public void Connect(Node source, Node destination) {
            source.Connect(destination);
        }

        internal IMFTopology MFTopology { get; private set; }
    }
}