#region Usings

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFAsyncResult), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFAsyncResult {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetState([MarshalAs(UnmanagedType.IUnknown)] out object ppunkState);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        // Suppress the HRESULT signature transformation and return the value to the caller instead
        [PreserveSig]
        int GetStatus();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetStatus([In, MarshalAs(UnmanagedType.Error)] int hrStatus);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetObject([MarshalAs(UnmanagedType.IUnknown)] out object ppObject);
    }
}