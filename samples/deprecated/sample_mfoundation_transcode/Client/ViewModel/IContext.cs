#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public interface IContext {
        event Action<Exception> Error;
        void ThrowError(Exception exception);
    }
}