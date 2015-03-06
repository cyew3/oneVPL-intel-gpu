namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class Facade : IFacade {
        public Facade() {
            Open();
        }

        ~Facade() {
            Close();
        }

        public SourceResolver CreateDShowSourceResolver() {
            return new SourceResolver(MFHelper.MFCreateDShowSourceResolver());
        }

        public SourceResolver CreateNativeSourceResolver() {
            return new SourceResolver(MFHelper.MFCreateSourceResolver());
        }

        public void Open() {
            MFHelper.MFStartup();
        }

        public void Close() {
            MFHelper.MFShutdown();
        }
    }
}