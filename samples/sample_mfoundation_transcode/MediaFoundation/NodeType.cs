namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public enum NodeType : uint {
        Output = 0,
        SourceStream,
        Transform,
        Tee,
        Max = uint.MaxValue
    }
}