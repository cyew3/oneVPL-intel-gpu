#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client {
    public partial class MainWindow {
        public MainWindow() {
            InitializeComponent();
            DataContext = new MainViewModel(new OpenFileDialog(), new SaveFileDialog(), new Context());
        }
    }
}