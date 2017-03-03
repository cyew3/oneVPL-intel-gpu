#region Usings

using System.IO;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class SaveFileDialog : ISaveFileDialog {
        public void Show(string fileName) {
            var dialog = new Microsoft.Win32.SaveFileDialog();
            dialog.FileName = Path.GetFileName(fileName);
            dialog.DefaultExt = ".mp4; .avi; .wmv; .mov";
            dialog.Filter = "Video files|*.mp4; *.avi; *.wmv; *.mov";

            var result = dialog.ShowDialog();

            if (result != true) return;

            FileName = dialog.FileName;
        }

        public string FileName { get; private set; }
    }
}