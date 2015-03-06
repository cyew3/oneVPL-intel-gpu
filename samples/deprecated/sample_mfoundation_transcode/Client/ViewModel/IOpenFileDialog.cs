namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public interface IOpenFileDialog {
        void Show();
        string FileName { get; }
    }
}