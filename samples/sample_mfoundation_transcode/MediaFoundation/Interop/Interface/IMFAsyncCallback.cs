#region Usings

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFAsyncCallback), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFAsyncCallback {
        // Suppress the HRESULT signature transformation and return the value to the caller instead
        [PreserveSig, MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        int GetParameters(out uint pdwFlags, out uint pdwQueue);

        // Suppress the HRESULT signature transformation and return the value to the caller instead
        [PreserveSig, MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        int Invoke([In, MarshalAs(UnmanagedType.Interface)] IMFAsyncResult pAsyncResult);
    }
}