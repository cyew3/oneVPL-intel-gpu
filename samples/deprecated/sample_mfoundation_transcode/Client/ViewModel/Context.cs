#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    internal class Context : IContext {
        public event Action<Exception> Error;

        public void ThrowError(Exception exception) {
            if (Error != null) Error(exception);
        }
    }
}