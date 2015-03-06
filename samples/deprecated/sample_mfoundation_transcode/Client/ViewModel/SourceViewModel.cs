#region Usings

using System;
using System.IO;
using System.Windows.Input;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class SourceViewModel : ViewModel {
        public SourceViewModel(IOpenFileDialog openFileDialog) {
            SelectSourceFile = new DelegateCommand(SelectSourceFileCommand);
            OpenFileDialog = openFileDialog;
        }

        private string sourceFileName;

        public string SourceFileName {
            get { return sourceFileName; }
            set {
                sourceFileName = value;
                OnPropertyChanged(() => SourceFileName);
            }
        }

        public string SourceFileFullPath { get; private set; }

        public ICommand SelectSourceFile { get; private set; }

        private void SelectSourceFileCommand() {
            OpenFileDialog.Show();
            SourceFileFullPath = OpenFileDialog.FileName;
            SourceFileName = Path.GetFileName(SourceFileFullPath);
            if (SourceFileSelected != null) SourceFileSelected();
        }

        public IOpenFileDialog OpenFileDialog { get; private set; }

        public event Action SourceFileSelected;
    }
}