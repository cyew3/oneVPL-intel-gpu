#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public abstract class Node {
        internal IMFTopologyNode MFNode { get; private set; }

        public NodeType Type { get; private set; }


        internal Node(NodeType type) {
            Type = type;
            MFNode = MFHelper.MFCreateTopologyNode(type);
        }

        public void Connect(Node node) {
            MFNode.ConnectOutput(0, node.MFNode, 0);
        }
    }
}