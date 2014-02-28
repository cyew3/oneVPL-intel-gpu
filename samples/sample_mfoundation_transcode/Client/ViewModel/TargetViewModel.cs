#region Usings

using System.IO;
using System.Windows.Input;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class TargetViewModel : ViewModel {
        public TargetViewModel(ISaveFileDialog saveFileDialog) {
            SelectTargetFile = new DelegateCommand(SelectTargetFileCommand);
            this.saveFileDialog = saveFileDialog;
        }

        public string TargetFileName {
            get { return targetFileName; }
            set {
                targetFileName = value;
                OnPropertyChanged(() => TargetFileName);
            }
        }

        public string TargetFileFullPath {
            get { return targetFileFullPath; }
            set {
                targetFileFullPath = value;
                TargetFileName = Path.GetFileName(targetFileFullPath);
            }
        }

        public bool IsEnabled {
            get { return isEnabled; }
            set {
                isEnabled = value;
                OnPropertyChanged(() => IsEnabled);
            }
        }

        public ICommand SelectTargetFile { get; private set; }

        private void SelectTargetFileCommand() {
            saveFileDialog.Show(TargetFileFullPath);
            TargetFileFullPath = saveFileDialog.FileName;
        }

        private readonly ISaveFileDialog saveFileDialog;
        private string targetFileName;
        private string targetFileFullPath;
        private bool isEnabled;
    }
}