namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class OpenFileDialog : IOpenFileDialog {
        public void Show() {
            var dialog = new Microsoft.Win32.OpenFileDialog();
            dialog.DefaultExt = ".mp4; .avi; .wmv; .mov; .mpg";
            dialog.Filter = "Video files|*.mp4; *.avi; *.wmv; *.mov; *.mpg";

            var result = dialog.ShowDialog();

            if (result != true) return;

            FileName = dialog.FileName;
        }

        public string FileName { get; private set; }
    }
}