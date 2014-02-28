namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    internal static class CastExtensions {
        internal static uint ToUint(this bool b) {
            return (uint) (b ? 1 : 0);
        }
    }
}