namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public interface IFacade {
        SourceResolver CreateDShowSourceResolver();
        SourceResolver CreateNativeSourceResolver();
        void Close();
    }
}