namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class VideoOutputNode : Node {
        public VideoOutputNode(Mpeg4MediaSink mediaSink, int index) : base(NodeType.Output) {
            var streamSink = mediaSink.GetStreamSinkByIndex(index);
            MFNode.SetObject(streamSink);
        }
    }
}