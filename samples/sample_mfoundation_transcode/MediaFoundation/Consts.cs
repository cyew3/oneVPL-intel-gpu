#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    internal static class Consts {
        internal const uint MF_RESOLUTION_MEDIASOURCE = 0x00000001;

        internal const uint MESessionTopologySet = 101;
        internal const uint MESessionClosed = 106;

        internal const int E_NOTIMPL = -2147467263;

        internal static Guid MF_MT_SUBTYPE = new Guid("f7e34c9a-42e8-4714-b74b-cb29d72c35e5");
    }
}