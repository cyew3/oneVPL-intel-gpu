namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class AudioOutputNode : Node {
        public AudioOutputNode(Mpeg4MediaSink mediaSink, int index) : base(NodeType.Output) {
            var streamSink = mediaSink.GetStreamSinkByIndex(index);
            MFNode.SetObject(streamSink);
        }
    }
}