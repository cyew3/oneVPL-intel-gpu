#region Usings

using System.Runtime.InteropServices;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Structure {
    [StructLayout(LayoutKind.Explicit)]
    internal class MediaSessionStartPosition {
        public MediaSessionStartPosition(long startPosition) {
            internalUse = 20; // VT_18
            this.startPosition = startPosition;
        }

        [FieldOffset(0)] public short internalUse;
        [FieldOffset(8)] public long startPosition;
    }
}