namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public interface ISaveFileDialog {
        void Show(string fileName);
        string FileName { get; }
    }
}