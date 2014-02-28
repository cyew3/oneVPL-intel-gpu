namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public enum InterlaceMode {
        Unknown = 0,
        Progressive = 2,
        FieldInterleavedUpperFirst = 3,
        FieldInterleavedLowerFirst = 4,
        FieldSingleUpper = 5,
        FieldSingleLower = 6,
        MixedInterlaceOrProgressive = 7,
        Last = (MixedInterlaceOrProgressive + 1),
        ForceDword = 0x7fffffff
    }
}