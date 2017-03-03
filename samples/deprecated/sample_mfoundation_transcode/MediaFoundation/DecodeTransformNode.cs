namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class DecodeTransformNode : TransformNode {
        public DecodeTransformNode(AudioFormat audioFormat) : base(new DecodeTransform(audioFormat)) {}
        public DecodeTransformNode(VideoFormat videoFormat) : base(new DecodeTransform(videoFormat)) {}
    }
}